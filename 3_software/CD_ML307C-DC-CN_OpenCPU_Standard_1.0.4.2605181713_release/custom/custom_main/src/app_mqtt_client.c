/**
 * @file    app_mqtt_client.c
 * @brief   MQTT 客户端实现：连接 / 订阅 / 发布 / 断线重连
 *          重连退避（需求 6.1）：5s → 10s → 30s → 60s 封顶，连接成功重置；
 *          看护/省电/休眠模式连续失败 APP_RECONN_MAX_FAIL_LP 次后停止自动
 *          重连，等待下一定位周期 app_mqtt_kick_reconnect() 触发，避免弱网
 *          环境持续耗电。退避计时用 RTC UTC 秒（LP 睡眠期间 OS tick 冻结）。
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "cm_os.h"
#include "cm_mqtt.h"
#include "cm_rtc.h"
#include "cm_modem_info.h"
#include "app_log.h"
#include "app_config.h"
#include "app_mode.h"
#include "app_mqtt_client.h"
#include "app_protocol.h"

static cm_mqtt_client_t    *s_client = NULL;
static osEventFlagsId_t     s_evt = NULL;
static osThreadId_t         s_task = NULL;
static app_mqtt_event_cb_t  s_user_cb = NULL;
static bool                 s_running = false;
static bool                 s_connected = false;
/* 保存最近一次连接凭证，用于 app_mqtt_disconnect 后重新连接 */
static app_mqtt_credential_t s_saved_cred;
static bool                 s_has_cred = false;

/* keepalive 秒数（需求 6.3 分模式；连接建立时生效） */
static uint16_t             s_keepalive_sec = APP_MQTT_KEEPALIVE_LP_SEC;

/* 重连退避状态（需求 6.1） */
static uint32_t             s_fail_count = 0;      /* 连续失败次数 */
static uint64_t             s_next_retry_utc = 0;  /* 下次允许重试时刻（UTC 秒） */
static bool                 s_lp_stop = false;     /* LP 模式达失败上限，停止自动重连 */

/* 发布缓冲区（声明前置：app_mqtt_init 需创建 s_pub_mutex，实现见文件后部） */
#define PUB_BUF_NUM   4
#define PUB_BUF_SIZE  1024
static char         s_pub_buf[PUB_BUF_NUM][PUB_BUF_SIZE];
static int          s_pub_idx = 0;
static osMutexId_t  s_pub_mutex = NULL;

#define EVT_CONNECTED    (1u << 0)
#define EVT_DISCONNECTED (1u << 1)

/* 退避阶梯：第 1/2/3 次失败后分别等 5/10/30s，其后 60s 封顶 */
static uint32_t backoff_sec(uint32_t fail_count)
{
    if (fail_count <= 1) return APP_RECONN_BACKOFF_1_S;
    if (fail_count == 2) return APP_RECONN_BACKOFF_2_S;
    if (fail_count == 3) return APP_RECONN_BACKOFF_3_S;
    return APP_RECONN_BACKOFF_MAX_S;
}

void app_mqtt_set_keepalive_sec(uint16_t sec)
{
    if (sec > 0) {
        s_keepalive_sec = sec;
    }
}

void app_mqtt_kick_reconnect(void)
{
    s_lp_stop = false;
    s_next_retry_utc = 0;   /* 立即允许一次重试（mqtt_task 1s 周期内执行） */
}

/* ====== 回调 ====== */
static int cb_connack(cm_mqtt_client_t *client, int session, cm_mqtt_conn_state_e conn_res)
{
    (void)client; (void)session;
    if (conn_res == CM_MQTT_CONN_STATE_SUCCESS) {
        s_connected = true;
        /* 连接成功重置退避状态（需求 6.1：5/10/30/60s 阶梯重新起算，
         * 并清除 LP 停止标志，恢复自动重连能力） */
        s_fail_count = 0;
        s_next_retry_utc = 0;
        s_lp_stop = false;
        osEventFlagsSet(s_evt, EVT_CONNECTED);
        if (s_user_cb) s_user_cb(APP_MQTT_EVT_CONNECTED, NULL);
        APP_LOGI("mqtt connected");
    } else {
        /* 非成功状态：客户端断开/服务器拒绝/ping 超时/网络异常等。
         * SDK 内部自动重连（RECONN_TIMES=3, RECONN_CYCLE=20s）会处理恢复，
         * 此处仅通知上层并更新状态。 */
        bool was_connected = s_connected;
        s_connected = false;
        APP_LOGW("mqtt connack fail:%d (was_connected=%d)", conn_res, was_connected);
        if (was_connected && s_user_cb) {
            s_user_cb(APP_MQTT_EVT_DISCONNECTED, NULL);
        }
    }
    return 0;
}

static int cb_publish(cm_mqtt_client_t *client, unsigned short msgid, char *topic,
                       int total_len, int payload_len, char *payload)
{
    (void)client; (void)msgid; (void)total_len;
    APP_LOGI("rx topic=%s len=%d", topic, payload_len);
    if (s_user_cb) {
        app_mqtt_msg_t msg;
        strncpy(msg.topic, topic, sizeof(msg.topic) - 1);
        msg.topic[sizeof(msg.topic) - 1] = '\0';
        msg.payload = payload;
        msg.payload_len = payload_len;
        s_user_cb(APP_MQTT_EVT_DATA_RX, &msg);
    }
    return 0;
}

static int cb_suback(cm_mqtt_client_t *client, unsigned short msgid, int count, int qos[])
{
    (void)client; (void)msgid; (void)count; (void)qos;
    if (s_user_cb) s_user_cb(APP_MQTT_EVT_SUBSCRIBED, NULL);
    APP_LOGI("mqtt subscribed");
    return 0;
}

static int cb_timeout(cm_mqtt_client_t *client, unsigned short msgid)
{
    (void)client; (void)msgid;
    APP_LOGW("mqtt pub timeout");
    return 0;
}

/* ====== 内部任务：监听断开状态 + 应用层重连（退避策略，需求 6.1）======
 * SDK 内部自动重连（RECONN_TIMES=3, RECONN_CYCLE=20s）仅处理网络级断连，
 * connack fail（服务器拒绝凭证）后 SDK 不会重试，由本任务按退避阶梯兜底。
 * LP 模式（看护/省电/休眠）连续失败 APP_RECONN_MAX_FAIL_LP 次后停止自动
 * 重连，等待下一定位周期唤醒时 app_mqtt_kick_reconnect() 触发。 */
static void mqtt_task(void *arg)
{
    (void)arg;
    while (s_running) {
        osDelay(APP_MS_TO_TICK(1000));
        if (!s_running || !s_client) continue;

        int st = cm_mqtt_client_get_state(s_client);
        if (st != CM_MQTT_STATE_DISCONNECTED) continue;

        if (s_connected) {
            /* 之前连着，刚断开 */
            s_connected = false;
            APP_LOGW("mqtt state poll detected disconnect");
            if (s_user_cb) s_user_cb(APP_MQTT_EVT_DISCONNECTED, NULL);
        }
        if (!s_has_cred) continue;

        /* LP 模式失败次数达上限：停止自动重连，等下一定位周期 kick */
        app_mode_e m = app_mode_get();
        bool lp_mode = (m == APP_MODE_SUPERVISE || m == APP_MODE_LOWPOWER ||
                        m == APP_MODE_SLEEP);
        if (lp_mode && s_fail_count >= APP_RECONN_MAX_FAIL_LP && !s_lp_stop) {
            s_lp_stop = true;
            APP_LOGW("mqtt reconnect fail %lu times in lp mode, wait next cycle kick",
                     (unsigned long)s_fail_count);
        }
        if (s_lp_stop) continue;

        /* 退避计时用 RTC UTC 秒：LP 睡眠期间 OS tick 冻结，tick 计时会失真 */
        uint64_t now_utc = cm_rtc_get_current_time();
        if (now_utc < s_next_retry_utc) continue;

        s_fail_count++;
        s_next_retry_utc = now_utc + backoff_sec(s_fail_count);
        APP_LOGI("mqtt app-level reconnect #%lu (next in %lus)",
                 (unsigned long)s_fail_count,
                 (unsigned long)backoff_sec(s_fail_count));
        app_mqtt_connect(&s_saved_cred);
    }
}

/* ====== 公开 API ====== */
int app_mqtt_init(app_mqtt_event_cb_t cb)
{
    s_user_cb = cb;
    if (s_evt == NULL) {
        osEventFlagsAttr_t attr = {0};
        attr.name = "mqtt_evt";
        s_evt = osEventFlagsNew(&attr);
    }
    if (s_pub_mutex == NULL) {
        osMutexAttr_t mattr = {0};
        mattr.name = "mqtt_pub";
        s_pub_mutex = osMutexNew(&mattr);
    }
    return 0;
}

int app_mqtt_connect(const app_mqtt_credential_t *cred)
{
    if (!cred) return -1;
    if (s_client) {
        cm_mqtt_client_disconnect(s_client);
        osDelay(APP_MS_TO_TICK(200));
        cm_mqtt_client_destroy(s_client);
        s_client = NULL;
    }
    /* 保存凭证用于主动重连兜底 */
    s_saved_cred = *cred;
    s_has_cred = true;

    s_client = cm_mqtt_client_create();
    if (!s_client) {
        APP_LOGE("mqtt client create fail");
        return -2;
    }

    /* 设置回调 */
    cm_mqtt_client_cb_t cbs = {0};
    cbs.connack_cb = cb_connack;
    cbs.publish_cb = cb_publish;
    cbs.suback_cb  = cb_suback;
    cbs.timeout_cb = cb_timeout;
    cm_mqtt_client_set_opt(s_client, CM_MQTT_OPT_EVENT, &cbs);

    /* 客户端参数：与 SDK 示例（examples/mqtt_unicom）保持一致。
     * 崩溃根因已定位为主循环高频调用 cm_adc_read/cm_modem_info_radio
     * 损坏 OSA tx 状态，与 SDK 定时器无关，已通过节流修复。 */
    int version = 4;        /* MQTT 3.1.1 */
    cm_mqtt_client_set_opt(s_client, CM_MQTT_OPT_VERSION, &version);
    int ping = s_keepalive_sec;         /* keepalive PING 周期（秒，分模式，需求 6.3） */
    cm_mqtt_client_set_opt(s_client, CM_MQTT_OPT_PING_CYCLE, &ping);
    int pkt_timeout = 10;   /* 发送超时 10 秒（QoS 1/2 消息可靠投递） */
    cm_mqtt_client_set_opt(s_client, CM_MQTT_OPT_PKT_TIMEOUT, &pkt_timeout);
    int retry_times = 3;    /* QoS 重传次数 */
    cm_mqtt_client_set_opt(s_client, CM_MQTT_OPT_RETRY_TIMES, &retry_times);
    int reconn_mode = 0;    /* 固定间隔重连 */
    cm_mqtt_client_set_opt(s_client, CM_MQTT_OPT_RECONN_MODE, &reconn_mode);
    int reconn_times = 3;   /* SDK 内部自动重连次数 */
    cm_mqtt_client_set_opt(s_client, CM_MQTT_OPT_RECONN_TIMES, &reconn_times);
    int reconn_cycle = 20;  /* 重连间隔 20 秒 */
    cm_mqtt_client_set_opt(s_client, CM_MQTT_OPT_RECONN_CYCLE, &reconn_cycle);
    int dns_priority = 1;   /* IPv4 优先 */
    cm_mqtt_client_set_opt(s_client, CM_MQTT_OPT_DNS_PRIORITY, &dns_priority);

    /* 连接：SDK 1.0.4 的 cm_mqtt_client_connect 为异步组包（客户端任务在
     * 函数返回后才读取 opt 中的指针），因此 opt 本身及 hostname/clientid/
     * username/password 必须指向长期有效的静态存储（s_saved_cred），
     * 不能使用调用者传入的栈上凭证，否则首连 CONNECT 包凭证字段为悬空数据 */
    static cm_mqtt_connect_options_t opt;
    memset(&opt, 0, sizeof(opt));
    opt.hostport = s_saved_cred.mqtt_port;
    opt.hostname = s_saved_cred.mqtt_host;
    opt.clientid = s_saved_cred.client_id;
    opt.username = s_saved_cred.username;
    opt.password = s_saved_cred.password;
    opt.keepalive = s_keepalive_sec;
    opt.clean_session = 1;

    APP_LOGI("mqtt connect %s:%u", s_saved_cred.mqtt_host, s_saved_cred.mqtt_port);
    int ret = cm_mqtt_client_connect(s_client, &opt);
    if (ret != 0) {
        APP_LOGE("mqtt connect ret=%d", ret);
    }

    /* 启动状态监控任务 */
    if (!s_running) {
        s_running = true;
        osThreadAttr_t tattr = {0};
        tattr.name = "mqtt_task";
        tattr.stack_size = 4 * 1024;
        tattr.priority = osPriorityBelowNormal1;   /* SDK 1.0.4 应用层优先级体系 */
        s_task = osThreadNew(mqtt_task, NULL, &tattr);
    }
    return ret;
}

int app_mqtt_disconnect(void)
{
    s_running = false;
    if (s_task) {
        osThreadTerminate(s_task);
        s_task = NULL;
    }
    if (s_client) {
        cm_mqtt_client_disconnect(s_client);
        osDelay(APP_MS_TO_TICK(200));
        cm_mqtt_client_destroy(s_client);
        s_client = NULL;
    }
    s_connected = false;
    s_has_cred = false;
    memset(&s_saved_cred, 0, sizeof(s_saved_cred));
    return 0;
}

bool app_mqtt_is_connected(void)
{
    return s_connected;
}

/* cm_mqtt_client_publish 是异步接口，内部不拷贝 payload，
 * 调用方在 publish 返回后不能立即释放 payload 内存。
 * 调用方含多任务（main_task / lbs_task / one_shot_loc_task）：
 * 1) 互斥锁保证 memcpy+publish 调用原子，消除并发交叉覆写；
 * 2) 4 槽轮换缓冲，降低前一条报文在 SDK 异步发送窗口（QoS1 重传）
 *    内被下一次 publish 覆写的概率。
 * （PUB_BUF_NUM/PUB_BUF_SIZE/s_pub_buf/s_pub_idx/s_pub_mutex 声明在文件前部） */
static char *pub_buf_acquire(void)
{
    if (s_pub_mutex) osMutexAcquire(s_pub_mutex, osWaitForever);
    char *buf = s_pub_buf[s_pub_idx];
    s_pub_idx = (s_pub_idx + 1) % PUB_BUF_NUM;
    return buf;
}

static void pub_buf_release(void)
{
    if (s_pub_mutex) osMutexRelease(s_pub_mutex);
}

int app_mqtt_publish_telemetry(const char *payload, int len)
{
    if (!s_client || !s_connected) return -1;
    if (!payload) return -2;
    if (len <= 0) len = (int)strlen(payload);
    if (len > PUB_BUF_SIZE) return -3;
    char *buf = pub_buf_acquire();
    memcpy(buf, payload, len);
    int ret = cm_mqtt_client_publish(s_client, APP_MQTT_TOPIC_TELEMETRY,
                                     buf, len, CM_MQTT_QOS_1);
    pub_buf_release();
    return ret;
}

int app_mqtt_publish_rpc_response(const char *request_id, const char *payload, int len)
{
    if (!s_client || !s_connected || !request_id) return -1;
    char topic[96];
    snprintf(topic, sizeof(topic), "%s%s", APP_MQTT_TOPIC_RPC_RESP, request_id);
    if (len <= 0 && payload) len = (int)strlen(payload);
    if (len > PUB_BUF_SIZE) return -3;
    char *buf = pub_buf_acquire();
    memcpy(buf, payload, len);
    int ret = cm_mqtt_client_publish(s_client, topic, buf, len, CM_MQTT_QOS_0);
    pub_buf_release();
    return ret;
}

/* 订阅：连接成功后由调用方触发 */
int app_mqtt_subscribe_rpc(void)
{
    if (!s_client) return -1;
    const char *topics[] = { APP_MQTT_TOPIC_RPC_REQ };
    const char qos[] = { 1 };
    return cm_mqtt_client_subscribe(s_client, topics, qos, 1);
}
