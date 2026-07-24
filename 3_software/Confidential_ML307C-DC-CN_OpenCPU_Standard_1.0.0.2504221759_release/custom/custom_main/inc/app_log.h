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
#define APP_LOGI(fmt, ...)   cm_log_printf(0, APP_LOG_TAG "[I] " fmt, ##__VA_ARGS__)
#define APP_LOGW(fmt, ...)   cm_log_printf(0, APP_LOG_TAG "[W] " fmt, ##__VA_ARGS__)
#define APP_LOGE(fmt, ...)   cm_log_printf(0, APP_LOG_TAG "[E] " fmt, ##__VA_ARGS__)
#define APP_LOGD(fmt, ...)   cm_log_printf(0, APP_LOG_TAG "[D] " fmt, ##__VA_ARGS__)
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
