/**
 * @file    app_storage.h
 * @brief   持久化存储：MQTT 凭证 / 离线定位缓存 / 序列号
 *          基于 cm_fs (LittleFS)，文件名不带路径。
 */
#ifndef __APP_STORAGE_H__
#define __APP_STORAGE_H__

#include <stdint.h>
#include <stdbool.h>
#include "app_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========== MQTT 凭证 ========== */
typedef struct {
    char    mqtt_host[64];
    uint16_t mqtt_port;
    char    client_id[32];
    char    username[64];
    char    password[32];
    char    credential_type[16];
    char    issued_at[24];
} app_mqtt_credential_t;

int  app_storage_load_credential(app_mqtt_credential_t *cred);
int  app_storage_save_credential(const app_mqtt_credential_t *cred);

/* ========== 工作模式掉电保存（需求 9：异常复位后恢复复位前工作模式）========== */
int  app_storage_save_work_mode(int mode);
/* 读取保存的工作模式；返回 <0 表示无有效保存（首次上电） */
int  app_storage_load_work_mode(int *mode);

/* ========== 离线定位缓存（环形）==========
 * 单文件存储，写满则覆盖最旧条目；最多 APP_OFFLINE_CACHE_MAX 条 */
int  app_storage_offline_push(const app_offline_record_t *rec);
int  app_storage_offline_pop(app_offline_record_t *rec);
int  app_storage_offline_count(void);
int  app_storage_offline_clear(void);

#ifdef __cplusplus
}
#endif
#endif /* __APP_STORAGE_H__ */
