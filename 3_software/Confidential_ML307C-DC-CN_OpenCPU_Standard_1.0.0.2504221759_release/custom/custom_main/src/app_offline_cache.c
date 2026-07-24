/**
 * @file    app_offline_cache.c
 * @brief   离线定位缓存环形结构实现
 *          存储：off_dat.bin（数据）+ off_idx.bin（索引头）
 *          结构：固定容量环形缓冲，写满覆盖最旧条目。
 */
#include <string.h>
#include <stdio.h>
#include "cm_fs.h"
#include "cm_os.h"
#include "app_log.h"
#include "app_config.h"
#include "app_offline_cache.h"
#include "app_storage.h"  /* 复用文件命名 */

#define FILE_OFF_DAT    "off_dat.bin"
#define FILE_OFF_IDX    "off_idx.bin"

typedef struct {
    uint32_t head;   /* 读位置（最旧） */
    uint32_t tail;   /* 写位置（最新+1） */
    uint32_t count;  /* 当前条数 */
} off_idx_t;

static off_idx_t s_idx = {0};
static osMutexId_t s_mtx = NULL;

static void lock(void)   { if (s_mtx) osMutexAcquire(s_mtx, osWaitForever); }
static void unlock(void) { if (s_mtx) osMutexRelease(s_mtx); }

int app_offline_cache_init(void)
{
    if (!s_mtx) {
        osMutexAttr_t a = {0};
        a.name = "off_mtx";
        s_mtx = osMutexNew(&a);
    }
    lock();
    int fd = cm_fs_open(FILE_OFF_IDX, CM_FS_RB);
    if (fd >= 0) {
        cm_fs_read(fd, &s_idx, sizeof(s_idx));
        cm_fs_close(fd);
        if (s_idx.count > APP_OFFLINE_CACHE_MAX) {
            /* 数据损坏，重置 */
            memset(&s_idx, 0, sizeof(s_idx));
        }
    } else {
        memset(&s_idx, 0, sizeof(s_idx));
    }
    unlock();
    APP_LOGI("offline cache init count=%u", s_idx.count);
    return 0;
}

static int __attribute__((unused)) write_record(uint32_t slot, const app_offline_record_t *rec)
{
    (void)slot; (void)rec;
    /* TODO 第3阶段：持久化到 off_dat.bin 实现 1000 条上限 */
    return 0;
}

/* 简化版环形：使用内存数组缓存（RAM 优化版，30 条约 18KB）
 * TODO 第3阶段：改为持久化到文件系统（off_dat.bin 分块读写），
 *               以支持需求文档要求的 1000 条上限 */
static app_offline_record_t s_ring[APP_OFFLINE_CACHE_MAX];

int app_offline_cache_push(const app_offline_record_t *rec)
{
    if (!rec) return -1;
    int ret = 0;
    lock();
    if (s_idx.count >= APP_OFFLINE_CACHE_MAX) {
        /* 覆盖最旧 */
        s_ring[s_idx.head] = *rec;
        s_idx.head = (s_idx.head + 1) % APP_OFFLINE_CACHE_MAX;
        s_idx.tail = (s_idx.tail + 1) % APP_OFFLINE_CACHE_MAX;
    } else {
        s_ring[s_idx.tail] = *rec;
        s_idx.tail = (s_idx.tail + 1) % APP_OFFLINE_CACHE_MAX;
        s_idx.count++;
    }
    /* 持久化索引 */
    int fd = cm_fs_open(FILE_OFF_IDX, CM_FS_WB);
    if (fd >= 0) {
        cm_fs_write(fd, &s_idx, sizeof(s_idx));
        cm_fs_close(fd);
    }
    unlock();
    return ret;
}

int app_offline_cache_pop(app_offline_record_t *rec)
{
    if (!rec) return -1;
    lock();
    if (s_idx.count == 0) {
        unlock();
        return -1;
    }
    *rec = s_ring[s_idx.head];
    s_idx.head = (s_idx.head + 1) % APP_OFFLINE_CACHE_MAX;
    s_idx.count--;
    int fd = cm_fs_open(FILE_OFF_IDX, CM_FS_WB);
    if (fd >= 0) {
        cm_fs_write(fd, &s_idx, sizeof(s_idx));
        cm_fs_close(fd);
    }
    unlock();
    return 0;
}

int app_offline_cache_count(void)
{
    int c;
    lock(); c = (int)s_idx.count; unlock();
    return c;
}

int app_offline_cache_clear(void)
{
    lock();
    memset(&s_idx, 0, sizeof(s_idx));
    cm_fs_delete(FILE_OFF_IDX);
    cm_fs_delete(FILE_OFF_DAT);
    unlock();
    return 0;
}

void app_offline_cache_replay(app_offline_replay_cb_t cb)
{
    if (!cb) return;
    app_offline_record_t rec;
    int sent = 0;
    while (app_offline_cache_pop(&rec) == 0) {
        cb(&rec);
        sent++;
        if (sent % APP_OFFLINE_REPLAY_RATE == 0) {
            osDelay(1000); /* 5 条/秒 */
        }
    }
    APP_LOGI("offline replay done, sent=%d", sent);
}
