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
    APP_PROV_ERR_NETWORK,        /* 网络失败 */
    APP_PROV_ERR_SIGNATURE,      /* 401 INVALID_DEVICE_SIGNATURE */
    APP_PROV_ERR_NOT_FOUND,      /* 404 DEVICE_NOT_FOUND */
    APP_PROV_ERR_FROZEN,         /* 409 DEVICE_FROZEN / VOIDED */
    APP_PROV_ERR_NOT_IN_TENANT,  /* 409 DEVICE_NOT_IN_TENANT */
    APP_PROV_ERR_UNKNOWN,
} app_prov_result_e;

/* 调用 provisioning 接口；成功时写入 cred */
app_prov_result_e app_provisioning_request(app_mqtt_credential_t *cred);

#ifdef __cplusplus
}
#endif
#endif /* __APP_PROVISIONING_H__ */
