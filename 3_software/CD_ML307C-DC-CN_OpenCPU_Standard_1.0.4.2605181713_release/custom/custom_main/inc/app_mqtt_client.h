/**
 * @file    app_mqtt_client.h
 * @brief   MQTT 客户端封装：连接/订阅/发布/断线重连/状态回调
 */
#ifndef __APP_MQTT_CLIENT_H__
#define __APP_MQTT_CLIENT_H__

#include <stdint.h>
#include <stdbool.h>
#include "cm_mqtt.h"
#include "app_storage.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_MQTT_EVT_DISCONNECTED = 0,
    APP_MQTT_EVT_CONNECTED,
    APP_MQTT_EVT_SUBSCRIBED,
    APP_MQTT_EVT_DATA_RX,        /* payload: char * */
} app_mqtt_event_e;

typedef struct {
    char    topic[64];
    char   *payload;
    int     payload_len;
} app_mqtt_msg_t;

typedef void (*app_mqtt_event_cb_t)(app_mqtt_event_e evt, void *data);

/* 初始化（注册回调） */
int  app_mqtt_init(app_mqtt_event_cb_t cb);

/* 使用本地凭证或新凭证连接 */
int  app_mqtt_connect(const app_mqtt_credential_t *cred);
int  app_mqtt_disconnect(void);

/* 设置 MQTT keepalive 秒数（需求 6.3 按工作模式区分：
 * 寻宠/遛宠 120s，看护/省电/休眠 600s）。
 * 已连接时本次连接不变，下次（重）连接生效 */
void app_mqtt_set_keepalive_sec(uint16_t sec);

/* LP 模式唤醒后的重连踢醒（需求 6.1：看护/省电/休眠连续失败
 * APP_RECONN_MAX_FAIL_LP 次后停止自动重连，等待下一定位周期由
 * 本接口触发重试）。非阻塞，实际重连由内部任务执行 */
void app_mqtt_kick_reconnect(void);

/* 当前是否已连接 */
bool app_mqtt_is_connected(void);

/* 发布 telemetry（QoS1） */
int  app_mqtt_publish_telemetry(const char *payload, int len);

/* 回复 ThingsBoard RPC：v1/devices/me/rpc/response/{requestId} */
int  app_mqtt_publish_rpc_response(const char *request_id, const char *payload, int len);

/* 订阅 RPC topic：v1/devices/me/rpc/request/+ */
int  app_mqtt_subscribe_rpc(void);

#ifdef __cplusplus
}
#endif
#endif /* __APP_MQTT_CLIENT_H__ */
