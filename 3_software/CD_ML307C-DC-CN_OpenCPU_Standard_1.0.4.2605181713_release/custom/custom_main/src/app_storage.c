/**
 * @file    app_storage.c
 * @brief   持久化存储实现（cm_fs + LittleFS）
 *          第1阶段：基础读写接口与文件命名约定；
 *          第2阶段将补全离线缓存的环形索引管理。
 */
#include <string.h>
#include <stdio.h>
#include "cm_fs.h"
#include "app_log.h"
#include "app_storage.h"
#include "app_config.h"

#define FILE_MQTT_CRED      "mqttcred.bin"
#define FILE_BOOT_INFO      "bootinfo.bin"
#define FILE_OFFLINE_IDX    "off_idx.bin"
#define FILE_OFFLINE_DAT    "off_dat.bin"

/* ===== 通用文件读写小工具 ===== */
static int storage_write_file(const char *name, const void *data, uint32_t len)
{
    int32_t fd = cm_fs_open(name, CM_FS_WB);
    if (fd < 0) {
        APP_LOGE("fs open(w) %s fail:%d", name, fd);
        return -1;
    }
    int32_t w = cm_fs_write(fd, data, len);
    cm_fs_close(fd);
    return (w == (int32_t)len) ? 0 : -1;
}

static int storage_read_file(const char *name, void *data, uint32_t expect_len)
{
    int32_t fd = cm_fs_open(name, CM_FS_RB);
    if (fd < 0) return -1; /* 文件不存在视为正常 */
    int32_t r = cm_fs_read(fd, data, expect_len);
    cm_fs_close(fd);
    return (r == (int32_t)expect_len) ? 0 : -1;
}

/* ========== MQTT 凭证 ========== */
int app_storage_load_credential(app_mqtt_credential_t *cred)
{
    if (!cred) return -1;
    memset(cred, 0, sizeof(*cred));
    return storage_read_file(FILE_MQTT_CRED, cred, sizeof(*cred));
}

int app_storage_save_credential(const app_mqtt_credential_t *cred)
{
    if (!cred) return -1;
    return storage_write_file(FILE_MQTT_CRED, cred, sizeof(*cred));
}

/* ========== 启动信息 ========== */
typedef struct {
    char    boot_id[32];
    uint32_t seq;
} boot_info_t;

int app_storage_load_boot_info(char *boot_id, size_t bid_len, uint32_t *seq)
{
    boot_info_t info = {0};
    int ret = storage_read_file(FILE_BOOT_INFO, &info, sizeof(info));
    if (ret == 0) {
        if (boot_id) {
            strncpy(boot_id, info.boot_id, bid_len - 1);
            boot_id[bid_len - 1] = '\0';
        }
        if (seq) *seq = info.seq;
    }
    return ret;
}

int app_storage_save_boot_info(const char *boot_id, uint32_t seq)
{
    boot_info_t info = {0};
    if (boot_id) {
        strncpy(info.boot_id, boot_id, sizeof(info.boot_id) - 1);
    }
    info.seq = seq;
    return storage_write_file(FILE_BOOT_INFO, &info, sizeof(info));
}

/* ========== 离线缓存：第2阶段实现完整环形结构 ==========
 * 第1阶段使用单文件追加 + 索引文件；写满后整体回卷。
 * 接口已声明但内部为占位，避免阻塞主流程编译。 */
static uint32_t s_offline_count = 0;

int app_storage_offline_push(const app_offline_record_t *rec)
{
    (void)rec;
    if (s_offline_count >= APP_OFFLINE_CACHE_MAX) {
        /* TODO: 第2阶段实现环形覆盖最旧条目 */
        return -1;
    }
    /* TODO: 第2阶段写入 off_dat.bin / off_idx.bin */
    s_offline_count++;
    return 0;
}

int app_storage_offline_pop(app_offline_record_t *rec)
{
    (void)rec;
    if (s_offline_count == 0) return -1;
    /* TODO: 第2阶段读取并删除最旧 */
    s_offline_count--;
    return 0;
}

int app_storage_offline_count(void)
{
    return (int)s_offline_count;
}

int app_storage_offline_clear(void)
{
    cm_fs_delete(FILE_OFFLINE_DAT);
    cm_fs_delete(FILE_OFFLINE_IDX);
    s_offline_count = 0;
    return 0;
}
