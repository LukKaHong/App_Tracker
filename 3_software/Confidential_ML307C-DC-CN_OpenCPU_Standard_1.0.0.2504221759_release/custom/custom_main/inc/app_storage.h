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

/* ========== 启动 ID 与序号 ========== */
int  app_storage_load_boot_info(char *boot_id, size_t bid_len, uint32_t *seq);
int  app_storage_save_boot_info(const char *boot_id, uint32_t seq);

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
