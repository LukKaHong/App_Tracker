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

/* ========== OTA 升级状态（联调协议 V1 2.3/2.5）==========
 * resume：UPDATING 前持久化的升级恢复信息；新固件启动自检后消费并清除。
 *   自检（MQTT 连接+订阅成功）后本机版本 == resume_version → 上报 UPDATED；
 *   不匹配（刷写失败/回滚）→ 上报 FAILED/INSTALL_FAILED，当前版本不变。
 * last_done：最后成功安装的任务（title+version+checksum 唯一标识），
 *   同一任务不得重复安装（协议 2.3）。 */
typedef struct {
    uint8_t  resume_pending;             /* 1 = 存在待自检恢复信息 */
    char     resume_title[48];
    char     resume_version[64];
    char     resume_checksum[65];        /* 64 hex + '\0' */
    uint32_t resume_size;
    uint8_t  last_done_valid;
    char     done_title[48];
    char     done_version[64];
    char     done_checksum[65];
} app_ota_persist_t;

int  app_storage_load_ota_state(app_ota_persist_t *st);
int  app_storage_save_ota_state(const app_ota_persist_t *st);

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
