/**
 * @file    app_command.c
 * @brief   云端指令处理：HIGH_FREQ / SOUND / LIGHT / LOCATION_FREQ / SHUTDOWN / OTA
 *          - SOUND/LIGHT 改为异步持续 duration_seconds
 *          - OTA 接入 app_ota
 *          - 超省电模式收到 HIGH_FREQ_START 改为单次定位+上报，不切模式
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "cm_os.h"
#include "cm_pm.h"
#include "cJSON.h"
#include "app_log.h"
#include "app_config.h"
#include "app_protocol.h"
#include "app_command.h"
#include "app_mode.h"
#include "app_mqtt_client.h"
#include "app_ota.h"
#include "bsp.h"

/* 由 custom_main.c 提供的实现声明：触发超省电下单次定位 */
extern void app_main_trigger_one_shot_location(void);

/* 全局上下文：当前 IMEI（由 custom_main 注入） */
static char g_imei[16] = "000000000000000";

void app_command_set_imei(const char *imei)
{
    if (imei) {
        strncpy(g_imei, imei, sizeof(g_imei) - 1);
        g_imei[sizeof(g_imei) - 1] = '\0';
    }
}

void app_command_send_result(const char *command_id, const char *status,
                              const char *failure_code, const char *failed_reason)
{
    if (!command_id || !status) return;
    char *json = NULL;
    int len = 0;
    if (app_protocol_build_command_result(g_imei, command_id, status,
                                          failure_code, failed_reason,
                                          &json, &len) == 0) {
        if (app_mqtt_is_connected()) {
            app_mqtt_publish_telemetry(json, len);
        }
        free(json);
    }
}

/* OTA 进度回调 */
static void ota_progress_cb(app_ota_state_e state, int percent)
{
    switch (state) {
    case APP_OTA_STATE_DOWNLOADING:
        APP_LOGI("ota downloading %d%%", percent);
        break;
    case APP_OTA_STATE_WRITING:
        APP_LOGI("ota writing %d%%", percent);
        break;
    case APP_OTA_STATE_DONE:
        APP_LOGI("ota done, system will reboot");
        break;
    case APP_OTA_STATE_FAILED:
        APP_LOGE("ota failed");
        break;
    default:
        break;
    }
}

/* 指令处理：根据 method 执行动作 */
static void dispatch(const char *method, cJSON *params, const char *command_id)
{
    if (!method || !command_id) {
        return;
    }
    if (strcmp(method, "HIGH_FREQUENCY_LOCATION_START") == 0) {
        /* 超省电模式下：收到定位指令后做一次定位+上报，不切模式（需求 1.2.4） */
        if (app_mode_get() == APP_MODE_SUPER_SAVE) {
            APP_LOGI("super-save one-shot location");
            app_main_trigger_one_shot_location();
        } else {
            app_mode_set(APP_MODE_FIND_DOG);
        }
        app_command_send_result(command_id, APP_CMD_ACK, NULL, NULL);
    } else if (strcmp(method, "HIGH_FREQUENCY_LOCATION_STOP") == 0) {
        app_mode_set(APP_MODE_NORMAL);
        app_command_send_result(command_id, APP_CMD_ACK, NULL, NULL);
    } else if (strcmp(method, "SOUND") == 0) {
        /* 持续响铃：每秒响一次，默认持续 2 分钟（需求 4）
         * duration_seconds <= 0 视为停止指令，避免 0 传入 bsp 层被解释为"持续响直到手动停止" */
        int duration = 120;
        if (params) {
            cJSON *d = cJSON_GetObjectItem(params, "duration_seconds");
            if (d) duration = d->valueint;
        }
        if (duration <= 0) {
            bsp_buzzer_stop();
        } else {
            bsp_buzzer_beep_async((uint32_t)duration);
        }
        app_command_send_result(command_id, APP_CMD_ACK, NULL, NULL);
    } else if (strcmp(method, "SOUND_STOP") == 0) {
        /* 需求 4：持续时间内收到停止指令，停止蜂鸣 */
        bsp_buzzer_stop();
        app_command_send_result(command_id, APP_CMD_ACK, NULL, NULL);
    } else if (strcmp(method, "LIGHT") == 0) {
        /* RGB 绿->红->蓝 交替快闪，默认持续 5 分钟（需求 5）
         * duration_seconds <= 0 视为停止指令 */
        int duration = 300;
        if (params) {
            cJSON *d = cJSON_GetObjectItem(params, "duration_seconds");
            if (d) duration = d->valueint;
        }
        if (duration <= 0) {
            bsp_rgb_stop_pattern();
        } else {
            bsp_rgb_set_pattern(BSP_RGB_PATTERN_PLATFORM_CMD, (uint32_t)duration);
        }
        app_command_send_result(command_id, APP_CMD_ACK, NULL, NULL);
    } else if (strcmp(method, "LIGHT_STOP") == 0) {
        /* 需求 5：持续时间内收到停止指令，停止快闪 */
        bsp_rgb_stop_pattern();
        app_command_send_result(command_id, APP_CMD_ACK, NULL, NULL);
    } else if (strcmp(method, "LOCATION_FREQUENCY") == 0) {
        /* 平台调整常规定位频率 */
        int interval = 120;
        if (params) {
            cJSON *i = cJSON_GetObjectItem(params, "interval_seconds");
            if (i) interval = i->valueint;
        }
        /* 这里仅记录日志，模式保持不变（频率已由模式管理控制）
         * 如需动态调整，可扩展 app_mode 接口 */
        APP_LOGI("LOCATION_FREQUENCY interval=%ds", interval);
        app_command_send_result(command_id, APP_CMD_ACK, NULL, NULL);
    } else if (strcmp(method, "SHUTDOWN") == 0) {
        app_command_send_result(command_id, APP_CMD_ACK, NULL, NULL);
        bsp_buzzer_stop();
        bsp_rgb_stop_pattern();
        osDelay(APP_MS_TO_TICK(500)); /* 等待 telemetry 发出 */
        cm_pm_poweroff();
    } else if (strcmp(method, "OTA") == 0) {
        /* 平台下发 OTA 升级指令，params 中含 url */
        const char *url = NULL;
        if (params) {
            cJSON *u = cJSON_GetObjectItem(params, "url");
            if (u && u->valuestring) url = u->valuestring;
        }
        if (!url || url[0] == '\0') {
            APP_LOGE("ota cmd missing url");
            app_command_send_result(command_id, APP_CMD_FAILED,
                                     "INVALID_PARAM", "ota url missing");
            return;
        }
        if (app_ota_is_running()) {
            APP_LOGW("ota already running");
            app_command_send_result(command_id, APP_CMD_FAILED,
                                     "BUSY", "ota already running");
            return;
        }
        int r = app_ota_start(url, ota_progress_cb);
        if (r == 0) {
            APP_LOGI("ota started: %s", url);
            app_command_send_result(command_id, APP_CMD_ACK, NULL, NULL);
        } else {
            APP_LOGE("ota start fail:%d", r);
            app_command_send_result(command_id, APP_CMD_FAILED,
                                     "START_FAIL", "ota task create fail");
        }
    } else {
        APP_LOGW("unknown method: %s", method);
        app_command_send_result(command_id, APP_CMD_FAILED,
                                 "UNSUPPORTED_COMMAND", "unknown method");
    }
}

void app_command_handle(const char *topic, const char *payload, int payload_len)
{
    if (!topic || !payload) return;
    char *method = NULL;
    char command_id[64] = {0};
    cJSON *params = NULL;

    if (app_protocol_parse_rpc(payload, payload_len, &method,
                                command_id, sizeof(command_id),
                                &params) != 0) {
        APP_LOGE("rpc parse fail");
        return;
    }

    APP_LOGI("rpc method=%s cmd_id=%s", method, command_id);

    /* 提取 ThingsBoard requestId 并回 RPC response */
    char request_id[32] = {0};
    if (app_protocol_extract_request_id(topic, request_id, sizeof(request_id)) == 0) {
        char rpc_resp[128];
        snprintf(rpc_resp, sizeof(rpc_resp),
                 "{\"success\":true,\"command_id\":\"%s\"}",
                 command_id);
        app_mqtt_publish_rpc_response(request_id, rpc_resp, 0);
    }

    /* 执行业务 */
    dispatch(method, params, command_id);

    if (params) cJSON_Delete(params);
    if (method) free(method);
}
