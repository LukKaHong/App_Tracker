/**
 * @file    app_offline_cache.h
 * @brief   离线定位缓存：环形结构（push/pop/count/clear）
 *          存储路径：off_dat.bin（数据） + off_idx.bin（索引头）
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

/* 触发限速补传：按 5 条/秒速率把所有缓存弹出并通过 cb 上报 */
typedef void (*app_offline_replay_cb_t)(const app_offline_record_t *rec);
void app_offline_cache_replay(app_offline_replay_cb_t cb);

#ifdef __cplusplus
}
#endif
#endif /* __APP_OFFLINE_CACHE_H__ */
