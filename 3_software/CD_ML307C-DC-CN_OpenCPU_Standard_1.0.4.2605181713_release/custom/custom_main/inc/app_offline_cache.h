/**
 * @file    app_offline_cache.h
 * @brief   离线定位缓存：环形结构（push/pop/count/clear）
 *          纯 RAM 缓存，不落盘（需求 6.2：重启后可丢失）
 *          最大 APP_OFFLINE_CACHE_MAX 条，写满覆盖最旧。
 */
#ifndef __APP_OFFLINE_CACHE_H__
#define __APP_OFFLINE_CACHE_H__

#include <stdint.h>
#include <stdbool.h>
#include "app_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

int  app_offline_cache_init(void);

/* 推入一条 telemetry 记录；返回 0 成功 */
int  app_offline_cache_push(const app_offline_record_t *rec);

/* 弹出最旧一条（按采集顺序）；返回 0 成功，-1 表示空 */
int  app_offline_cache_pop(app_offline_record_t *rec);

/* 当前缓存条数 */
int  app_offline_cache_count(void);

/* 清空缓存 */
int  app_offline_cache_clear(void);

/* 增量补传：每次调用弹出最旧一条并通过 cb 上报
 * 返回 0=已弹出一条且仍有数据；1=缓存已空，补传完成；-1=参数错误
 * 调用方应在主循环中周期性调用直到返回 1，避免长时间阻塞主任务 */
typedef void (*app_offline_replay_cb_t)(const app_offline_record_t *rec);
int  app_offline_cache_replay_step(app_offline_replay_cb_t cb);

#ifdef __cplusplus
}
#endif
#endif /* __APP_OFFLINE_CACHE_H__ */
