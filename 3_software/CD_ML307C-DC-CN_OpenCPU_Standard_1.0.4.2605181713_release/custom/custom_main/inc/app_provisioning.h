/**
 * @file    app_provisioning.h
 * @brief   HTTP POST /api/device/v1/provision 获取 MQTT 凭证
 */
#ifndef __APP_PROVISIONING_H__
#define __APP_PROVISIONING_H__

#include <stdint.h>
#include "app_storage.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_PROV_OK = 0,
    /* --- 可重试类 --- */
    APP_PROV_ERR_NETWORK,        /* 网络超时/断网/HTTP 5xx：指数退避后重试 */
    APP_PROV_ERR_NONCE_REPLAY,   /* DEVICE_NONCE_REPLAY：换 nonce 立即重试 */
    /* --- 致命类（停止重试，需人工排查固件字段/型号密钥/设备状态）--- */
    APP_PROV_ERR_SIGNATURE,      /* INVALID_DEVICE_SIGNATURE：检查型号密钥/canonical/model_code */
    APP_PROV_ERR_VALIDATION,     /* VALIDATION_ERROR：检查固件请求字段和设备时间 */
    APP_PROV_ERR_NOT_FOUND,      /* DEVICE_NOT_FOUND：交由产测或售后处理 */
    APP_PROV_ERR_MODEL_MISMATCH, /* DEVICE_MODEL_MISMATCH：检查烧录型号 */
    APP_PROV_ERR_NOT_IN_TENANT,  /* DEVICE_NOT_IN_TENANT / INVALID_DEVICE_STATUS */
    APP_PROV_ERR_FROZEN,         /* DEVICE_FROZEN / VOIDED：不得继续联网 */
    /* --- 其他 --- */
    APP_PROV_ERR_UNKNOWN,
    /* 本地校验失败（响应字段不合法/凭证保存失败），流程层重新签名重试 */
    APP_PROV_ERR_LOCAL_INVALID,
} app_prov_result_e;

/* 错误是否可自动重试（联调协议 V1 1.6 重试矩阵） */
bool app_prov_is_retryable(app_prov_result_e r);

/* 调用 provisioning 接口；成功时写入 cred（已通过联调协议 1.4 的
 * 4 项响应校验：credential_type/port 范围/client_id==device_sn/username 非空） */
app_prov_result_e app_provisioning_request(app_mqtt_credential_t *cred);

#ifdef __cplusplus
}
#endif
#endif /* __APP_PROVISIONING_H__ */
