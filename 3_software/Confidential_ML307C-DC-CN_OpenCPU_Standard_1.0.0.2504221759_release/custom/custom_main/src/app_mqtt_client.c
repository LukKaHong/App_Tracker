/**
 * @file    app_mqtt_client.c
 * @brief   MQTT 客户端实现：连接 / 订阅 / 发布 / 断线重连
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "cm_os.h"
#include "cm_mqtt.h"
#include "cm_modem_info.h"
#include "app_log.h"
#include "app_config.h"
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

#define EVT_CONNECTED    (1u << 0)
#define EVT_DISCONNECTED (1u << 1)

/* ====== 回调 ====== */
static int cb_connack(cm_mqtt_client_t *client, int session, cm_mqtt_conn_state_e conn_res)
{
    (void)client; (void)session;
    if (conn_res == CM_MQTT_CONN_STATE_SUCCESS) {
        s_connected = true;
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

/* ====== 内部任务：监听断开状态 + 应用层重连 ======
 * SDK 内部自动重连（RECONN_TIMES=3, RECONN_CYCLE=20s）仅处理网络级断连，
 * connack fail（服务器拒绝凭证）后 SDK 不会重试。
 * 本任务每 60s 调用 app_mqtt_connect 兜底重连，无限重试直到连上。 */
static void mqtt_task(void *arg)
{
    (void)arg;
    uint32_t disconnect_tick = 0;
    while (s_running) {
        osDelay(APP_MS_TO_TICK(1000));
        if (!s_running || !s_client) continue;

        uint32_t now = (uint32_t)osKernelGetTickCount();
        int st = cm_mqtt_client_get_state(s_client);
        if (st == CM_MQTT_STATE_DISCONNECTED) {
            if (s_connected) {
                /* 之前连着，刚断开 */
                s_connected = false;
                disconnect_tick = now;
                APP_LOGW("mqtt state poll detected disconnect");
                if (s_user_cb) s_user_cb(APP_MQTT_EVT_DISCONNECTED, NULL);
            } else if (disconnect_tick == 0) {
                /* 初始连接失败（connack fail），从未连上过 */
                disconnect_tick = now;
            }
            /* 应用层重连：connack fail 后 SDK 不重试，每 60s 兜底 */
            if (s_has_cred && disconnect_tick > 0 &&
                (now - disconnect_tick) >= APP_MS_TO_TICK(60000)) {
                disconnect_tick = now;
                APP_LOGI("mqtt app-level reconnect");
                app_mqtt_connect(&s_saved_cred);
            }
        }
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
    int ping = APP_MQTT_KEEPALIVE_SEC;  /* keepalive PING 周期（秒） */
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

    /* 连接 */
    cm_mqtt_connect_options_t opt = {0};
    opt.hostport = cred->mqtt_port;
    opt.hostname = cred->mqtt_host;
    opt.clientid = cred->client_id;
    opt.username = cred->username;
    opt.password = cred->password;
    opt.keepalive = APP_MQTT_KEEPALIVE_SEC;
    opt.clean_session = 1;

    APP_LOGI("mqtt connect %s:%u", cred->mqtt_host, cred->mqtt_port);
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
        tattr.priority = osPriorityBelowNormal;
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
 * 用静态缓冲区拷贝一份，让调用方可以安全 free。 */
static char s_pub_buf[1024];

int app_mqtt_publish_telemetry(const char *payload, int len)
{
    if (!s_client || !s_connected) return -1;
    if (!payload) return -2;
    if (len <= 0) len = (int)strlen(payload);
    if (len > (int)sizeof(s_pub_buf)) return -3;
    memcpy(s_pub_buf, payload, len);
    return cm_mqtt_client_publish(s_client, APP_MQTT_TOPIC_TELEMETRY,
                                  s_pub_buf, len, CM_MQTT_QOS_1);
}

int app_mqtt_publish_rpc_response(const char *request_id, const char *payload, int len)
{
    if (!s_client || !s_connected || !request_id) return -1;
    char topic[96];
    snprintf(topic, sizeof(topic), "%s%s", APP_MQTT_TOPIC_RPC_RESP, request_id);
    if (len <= 0 && payload) len = (int)strlen(payload);
    if (len > (int)sizeof(s_pub_buf)) return -3;
    memcpy(s_pub_buf, payload, len);
    return cm_mqtt_client_publish(s_client, topic, s_pub_buf, len, CM_MQTT_QOS_0);
}

/* 订阅：连接成功后由调用方触发 */
int app_mqtt_subscribe_rpc(void)
{
    if (!s_client) return -1;
    const char *topics[] = { APP_MQTT_TOPIC_RPC_REQ };
    const char qos[] = { 1 };
    return cm_mqtt_client_subscribe(s_client, topics, qos, 1);
}
