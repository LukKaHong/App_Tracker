/**
 * @file    app_offline_cache.c
 * @brief   离线定位缓存环形结构实现（纯 RAM 缓存，不落盘）
 *          结构：固定容量环形缓冲，写满覆盖最旧条目。
 *          需求 6.2：离线数据重启后可丢失，不做落盘处理
 */
#include <string.h>
#include <stdio.h>
#include "cm_os.h"
#include "app_log.h"
#include "app_config.h"
#include "app_offline_cache.h"

typedef struct {
    uint32_t head;   /* 读位置（最旧） */
    uint32_t tail;   /* 写位置（最新+1） */
    uint32_t count;  /* 当前条数 */
} off_idx_t;

static off_idx_t s_idx = {0};
static osMutexId_t s_mtx = NULL;

/* 简化版环形：使用内存数组缓存（RAM 优化版，30 条约 18KB）
 * 需求 6.2：重启后可丢失，不持久化到文件系统 */
static app_offline_record_t s_ring[APP_OFFLINE_CACHE_MAX];

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
    /* 需求 6.2：离线缓存纯 RAM，重启后总是从空开始，避免索引与数据不一致 */
    memset(&s_idx, 0, sizeof(s_idx));
    memset(s_ring, 0, sizeof(s_ring));
    unlock();
    APP_LOGI("offline cache init (ram-only) count=%u", s_idx.count);
    return 0;
}

int app_offline_cache_push(const app_offline_record_t *rec)
{
    if (!rec) return -1;
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
    unlock();
    return 0;
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
    memset(s_ring, 0, sizeof(s_ring));
    unlock();
    return 0;
}

int app_offline_cache_replay_step(app_offline_replay_cb_t cb)
{
    if (!cb) return -1;
    app_offline_record_t rec;
    if (app_offline_cache_pop(&rec) != 0) {
        return 1;  /* 缓存已空，补传完成 */
    }
    cb(&rec);
    return 0;  /* 已弹出一条，可能还有更多 */
}
