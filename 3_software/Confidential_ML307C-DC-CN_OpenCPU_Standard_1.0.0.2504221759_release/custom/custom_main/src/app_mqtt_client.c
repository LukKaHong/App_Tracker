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
static volatile bool        s_need_reconnect = false;

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
        s_connected = false;
        APP_LOGW("mqtt connack fail:%d", conn_res);
        s_need_reconnect = true;
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

/* ====== 内部任务：监听断开并重连 ====== */
static void mqtt_task(void *arg)
{
    (void)arg;
    while (s_running) {
        if (s_connected && !s_need_reconnect) {
            osDelay(APP_MS_TO_TICK(1000));
            continue;
        }
        if (!s_need_reconnect) {
            osDelay(APP_MS_TO_TICK(1000));
            continue;
        }
        s_need_reconnect = false;
        APP_LOGI("mqtt reconnect in 5s");
        osDelay(APP_MS_TO_TICK(5000));
        if (!s_running) break;
        if (s_client) {
            int st = cm_mqtt_client_get_state(s_client);
            if (st == CM_MQTT_STATE_DISCONNECTED ||
                st == CM_MQTT_STATE_CONNECTING ||
                st == CM_MQTT_STATE_RECONNECTING) {
                /* 客户端内部已有重连机制，这里仅触发应用层状态恢复 */
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

    /* 心跳周期 / 超时 */
    int ping = APP_MQTT_KEEPALIVE_SEC;
    cm_mqtt_client_set_opt(s_client, CM_MQTT_OPT_PING_CYCLE, &ping);
    int reconn_mode = 1; /* 指数退避 */
    cm_mqtt_client_set_opt(s_client, CM_MQTT_OPT_RECONN_MODE, &reconn_mode);
    int reconn_cycle = 10;
    cm_mqtt_client_set_opt(s_client, CM_MQTT_OPT_RECONN_CYCLE, &reconn_cycle);

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
        s_need_reconnect = true;
    }

    /* 启动重连监控任务 */
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
    return 0;
}

bool app_mqtt_is_connected(void)
{
    return s_connected;
}

int app_mqtt_publish_telemetry(const char *payload, int len)
{
    if (!s_client || !s_connected) return -1;
    if (!payload) return -2;
    if (len <= 0) len = (int)strlen(payload);
    return cm_mqtt_client_publish(s_client, APP_MQTT_TOPIC_TELEMETRY,
                                  payload, len, CM_MQTT_QOS_1);
}

int app_mqtt_publish_rpc_response(const char *request_id, const char *payload, int len)
{
    if (!s_client || !s_connected || !request_id) return -1;
    char topic[96];
    snprintf(topic, sizeof(topic), "%s%s", APP_MQTT_TOPIC_RPC_RESP, request_id);
    if (len <= 0 && payload) len = (int)strlen(payload);
    return cm_mqtt_client_publish(s_client, topic, payload, len, CM_MQTT_QOS_0);
}

/* 订阅：连接成功后由调用方触发 */
int app_mqtt_subscribe_rpc(void)
{
    if (!s_client) return -1;
    const char *topics[] = { APP_MQTT_TOPIC_RPC_REQ };
    const char qos[] = { 1 };
    return cm_mqtt_client_subscribe(s_client, topics, qos, 1);
}
