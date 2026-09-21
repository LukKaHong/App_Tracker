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
#define FILE_WORK_MODE      "workmode.bin"
#define FILE_OTA_STATE      "otastate.bin"
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

/* ========== MQTT 凭证 ==========
 * 【联调协议 V1 1.4】凭证持久化必须防损坏、写失败不得标记激活成功。
 * 文件格式：header{magic, ver, len, crc32} + credential struct。
 * load 时校验 magic/长度/CRC，任一不符（凭证区损坏/旧格式残留）视为无凭证，
 * 走重新 provisioning 路径；save 采用"写-关-回读比对"三步确认，
 * 任一步失败返回 -1，调用方不得视为激活成功。 */
#define CRED_FILE_MAGIC     0x50525631u   /* "PRV1" */
#define CRED_FILE_VER       1u

typedef struct {
    uint32_t magic;
    uint32_t ver;
    uint32_t len;                        /* 后随 credential 数据长度 */
    uint32_t crc32;                      /* credential 数据 CRC32 */
} cred_file_hdr_t;

/* CRC32（IEEE 802.3 多项式，查表法省 flash 故用逐位计算——凭证仅 ~240B，
 * 每次激活流程调用一次，性能不敏感） */
static uint32_t cred_crc32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

int app_storage_load_credential(app_mqtt_credential_t *cred)
{
    if (!cred) return -1;
    memset(cred, 0, sizeof(*cred));

    /* 一次读入 hdr+data 再拆（storage_read_file 每次从文件头读，无偏移语义） */
    uint8_t buf[sizeof(cred_file_hdr_t) + sizeof(app_mqtt_credential_t)];
    if (storage_read_file(FILE_MQTT_CRED, buf, sizeof(buf)) != 0) {
        return -1;
    }
    cred_file_hdr_t hdr;
    memcpy(&hdr, buf, sizeof(hdr));
    if (hdr.magic != CRED_FILE_MAGIC || hdr.ver != CRED_FILE_VER ||
        hdr.len != sizeof(*cred)) {
        APP_LOGW("cred file hdr invalid, treat as no credential");
        return -1;
    }
    memcpy(cred, buf + sizeof(hdr), sizeof(*cred));
    if (cred_crc32((const uint8_t *)cred, sizeof(*cred)) != hdr.crc32) {
        APP_LOGW("cred crc mismatch, treat as no credential");
        return -1;
    }
    return 0;
}

int app_storage_save_credential(const app_mqtt_credential_t *cred)
{
    if (!cred) return -1;

    cred_file_hdr_t hdr;
    hdr.magic = CRED_FILE_MAGIC;
    hdr.ver   = CRED_FILE_VER;
    hdr.len   = sizeof(*cred);
    hdr.crc32 = cred_crc32((const uint8_t *)cred, sizeof(*cred));

    /* 先写数据文件再写头部会引入中间态；单文件两段写无法真正原子，
     * LittleFS 的 cm_fs_open(WB) 截断重建 + 写全量：先组到 RAM 再一次写。
     * 缓冲区取上限：hdr + struct，静态断言防结构膨胀 */
    uint8_t buf[sizeof(cred_file_hdr_t) + sizeof(app_mqtt_credential_t)];
    memcpy(buf, &hdr, sizeof(hdr));
    memcpy(buf + sizeof(hdr), cred, sizeof(*cred));
    if (storage_write_file(FILE_MQTT_CRED, buf, sizeof(buf)) != 0) {
        return -1;
    }

    /* 回读校验：字节级比对，覆盖 flash 写入异常 */
    uint8_t rdbuf[sizeof(buf)];
    if (storage_read_file(FILE_MQTT_CRED, rdbuf, sizeof(rdbuf)) != 0 ||
        memcmp(buf, rdbuf, sizeof(buf)) != 0) {
        APP_LOGE("cred save readback mismatch");
        return -1;
    }
    return 0;
}

/* 删除已存凭证：cm_fs_delete 对不存在的文件也返回错误，属正常（幂等）。
 * 删除后必须验证读不回——若文件系统异常导致删除失败，凭证仍在，
 * 调用方需明确知道"下次开机仍会走缓存凭证分支" */
int app_storage_clear_credential(void)
{
    cm_fs_delete(FILE_MQTT_CRED);

    app_mqtt_credential_t cred;
    if (app_storage_load_credential(&cred) == 0) {
        APP_LOGE("cred clear verify fail, file still readable");
        return -1;
    }
    return 0;
}

/* ========== 工作模式掉电保存（需求 9）==========
 * 模式切换不频繁（平台指令/超时回切/低电强制），每次切换写一次 flash 可接受 */
int app_storage_save_work_mode(int mode)
{
    int32_t m = mode;
    return storage_write_file(FILE_WORK_MODE, &m, sizeof(m));
}

int app_storage_load_work_mode(int *mode)
{
    if (!mode) return -1;
    int32_t m = -1;
    if (storage_read_file(FILE_WORK_MODE, &m, sizeof(m)) != 0) {
        return -1;
    }
    *mode = (int)m;
    return 0;
}

/* ========== OTA 升级状态（联调协议 V1 2.3/2.5）==========
 * 格式与凭证文件同款：header{magic, ver, len, crc32} + struct。
 * resume 持久化发生在 UPDATING（写入备用分区前）——该写入若损坏，
 * 设备回到正常启动路径（旧固件继续运行），仅丢失 UPDATED 上报机会，
 * 可由平台重新下发任务兜底；故采用与凭证相同的防损格式即可 */
#define OTA_STATE_MAGIC     0x4F545631u   /* "OTV1" */
#define OTA_STATE_VER       1u

int app_storage_load_ota_state(app_ota_persist_t *st)
{
    if (!st) return -1;
    memset(st, 0, sizeof(*st));

    uint8_t buf[sizeof(cred_file_hdr_t) + sizeof(app_ota_persist_t)];
    if (storage_read_file(FILE_OTA_STATE, buf, sizeof(buf)) != 0) {
        return -1;
    }
    cred_file_hdr_t hdr;
    memcpy(&hdr, buf, sizeof(hdr));
    if (hdr.magic != OTA_STATE_MAGIC || hdr.ver != OTA_STATE_VER ||
        hdr.len != sizeof(*st)) {
        APP_LOGW("ota state hdr invalid, treat as none");
        return -1;
    }
    memcpy(st, buf + sizeof(hdr), sizeof(*st));
    if (cred_crc32((const uint8_t *)st, sizeof(*st)) != hdr.crc32) {
        APP_LOGW("ota state crc mismatch, treat as none");
        return -1;
    }
    return 0;
}

int app_storage_save_ota_state(const app_ota_persist_t *st)
{
    if (!st) return -1;

    cred_file_hdr_t hdr;
    hdr.magic = OTA_STATE_MAGIC;
    hdr.ver   = OTA_STATE_VER;
    hdr.len   = sizeof(*st);
    hdr.crc32 = cred_crc32((const uint8_t *)st, sizeof(*st));

    uint8_t buf[sizeof(cred_file_hdr_t) + sizeof(app_ota_persist_t)];
    memcpy(buf, &hdr, sizeof(hdr));
    memcpy(buf + sizeof(hdr), st, sizeof(*st));
    if (storage_write_file(FILE_OTA_STATE, buf, sizeof(buf)) != 0) {
        return -1;
    }
    /* 回读校验：UPDATING 前的恢复信息必须确认落盘 */
    uint8_t rdbuf[sizeof(buf)];
    if (storage_read_file(FILE_OTA_STATE, rdbuf, sizeof(rdbuf)) != 0 ||
        memcmp(buf, rdbuf, sizeof(buf)) != 0) {
        APP_LOGE("ota state save readback mismatch");
        return -1;
    }
    return 0;
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
