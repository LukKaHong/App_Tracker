/**
 * @file    app_log.h
 * @brief   应用层日志宏
 * @details cm_log_printf 内部 buff 仅 180 字节，单条 log 不能超过 108 字节，
 *          因此封装时做长度截断保护。
 */
#ifndef __APP_LOG_H__
#define __APP_LOG_H__

#include "cm_sys.h"
#include "app_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#if APP_LOG_ENABLED
void app_log_output(const char *level, const char *fmt, ...);
/* 编译时级别过滤：APP_LOG_LEVEL 以下级别的日志被编译排除，零运行时开销 */
#if APP_LOG_LEVEL >= APP_LOG_LEVEL_INFO
#define APP_LOGI(fmt, ...)   app_log_output("I", fmt, ##__VA_ARGS__)
#else
#define APP_LOGI(fmt, ...)
#endif
#if APP_LOG_LEVEL >= APP_LOG_LEVEL_WARN
#define APP_LOGW(fmt, ...)   app_log_output("W", fmt, ##__VA_ARGS__)
#else
#define APP_LOGW(fmt, ...)
#endif
#if APP_LOG_LEVEL >= APP_LOG_LEVEL_ERROR
#define APP_LOGE(fmt, ...)   app_log_output("E", fmt, ##__VA_ARGS__)
#else
#define APP_LOGE(fmt, ...)
#endif
#if APP_LOG_LEVEL >= APP_LOG_LEVEL_DEBUG
#define APP_LOGD(fmt, ...)   app_log_output("D", fmt, ##__VA_ARGS__)
#else
#define APP_LOGD(fmt, ...)
#endif
#else
#define APP_LOGI(fmt, ...)
#define APP_LOGW(fmt, ...)
#define APP_LOGE(fmt, ...)
#define APP_LOGD(fmt, ...)
#endif

#ifdef __cplusplus
}
#endif
#endif /* __APP_LOG_H__ */
