/**
 * @file    app_ota.h
 * @brief   全系统差分升级：HTTP 下载 + cm_ota_* 写入 + 触发升级
 */
#ifndef __APP_OTA_H__
#define __APP_OTA_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_OTA_STATE_IDLE = 0,
    APP_OTA_STATE_DOWNLOADING,
    APP_OTA_STATE_WRITING,
    APP_OTA_STATE_DONE,
    APP_OTA_STATE_FAILED,
} app_ota_state_e;

typedef void (*app_ota_progress_cb_t)(app_ota_state_e state, int percent);

/* 启动 OTA 升级流程：从 url 下载差分包 -> cm_ota 写入 -> 触发升级
 * url 必须为完整 HTTP/HTTPS URL
 * 返回 0 表示流程已启动，结果通过 cb 异步上报 */
int  app_ota_start(const char *url, app_ota_progress_cb_t cb);

/* 当前是否正在升级 */
bool app_ota_is_running(void);

#ifdef __cplusplus
}
#endif
#endif /* __APP_OTA_H__ */
