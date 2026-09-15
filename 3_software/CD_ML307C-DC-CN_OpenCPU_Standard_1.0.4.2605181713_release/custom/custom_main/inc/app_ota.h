/**
 * @file    app_ota.h
 * @brief   APP 整包升级（需求 V1.30）：HTTP Range 分段下载 + cm_ota 分段写入 + 触发升级
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

/* 启动 OTA 升级流程：从 url 下载 APP 整包（HTTP Range 分段）-> cm_ota
 * 分段写入 -> 触发升级。url 必须为完整 HTTP/HTTPS URL，升级服务器须
 * 支持 Range 断点续传。
 * 返回 0 流程已启动（结果经 cb 异步上报）；
 *       -1 参数错误或已有升级在进行；
 *       -2 内部资源创建失败；
 *       -3 低电拒绝（SOC 低于超低电阈值且非充电中） */
int  app_ota_start(const char *url, app_ota_progress_cb_t cb);

/* 当前是否正在升级（升级期间主循环冻结 LP 睡眠与定位周期） */
bool app_ota_is_running(void);

#ifdef __cplusplus
}
#endif
#endif /* __APP_OTA_H__ */
