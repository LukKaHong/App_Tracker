/**
 * @file    app_command.c
 * @brief   云端指令处理：DEVICE_MODE / HIGH_FREQ / SOUND / LIGHT /
 *          LOCATION_FREQUENCY / SHUTDOWN / GET_VERSION / OTA
 *          - DEVICE_MODE：平台模式切换（需求 1 / 硬件协议 DEVICE_MODE）
 *          - SOUND/LIGHT 异步持续 duration_seconds，默认 30 秒（硬件协议）
 *          - LOCATION_FREQUENCY 生效：覆盖常规定位周期（平台协议 6）
 *          - SHUTDOWN：上报 OFFLINE 后 cm_pm_poweroff 断电关机（需求 V1.8：
 *            软件关机模式已删除，平台关机指令等价于硬件断电关机）
 *          - GET_VERSION：应答当前版本信息（需求 6.7 平台查询）
 *          - 按 command_id 幂等去重（硬件协议 4：重复指令仅重发缓存结果）
 *          - 失败码符合硬件协议 5：INVALID_PARAMETER/DEVICE_BUSY/INTERNAL_ERROR
 *          - 全部使用静态缓冲区，不使用 cJSON malloc/free（避免与 cmmqtt-m 堆冲突）
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "cm_os.h"
#include "app_log.h"
#include "app_config.h"
#include "app_protocol.h"
#include "app_command.h"
#include "app_mode.h"
#include "app_mqtt_client.h"
#include "app_ota.h"
#include "bsp.h"

/* 由 custom_main.c 提供的实现声明 */
extern void app_main_trigger_one_shot_location(void);
extern void app_main_execute_poweroff(void);
extern void app_main_report_device_info(void);

/* 全局上下文：当前 IMEI（由 custom_main 注入） */
static char g_imei[16] = "000000000000000";

/* ===== 指令幂等去重（硬件协议 4：相同 command_id 不重复执行，仅重发结果）===== */
#define APP_CMD_DEDUP_MAX  16
typedef struct {
    char command_id[64];
    bool ack;                    /* true=ACKNOWLEDGED，false=FAILED */
    char failure_code[24];       /* FAILED 时的稳定错误码 */
} dedup_entry_t;
static dedup_entry_t s_dedup[APP_CMD_DEDUP_MAX];
static int s_dedup_head = 0;     /* 最旧位置（覆盖点） */
static int s_dedup_count = 0;

/* 查找历史指令：找到返回索引，未找到返回 -1 */
static int dedup_lookup(const char *command_id)
{
    for (int i = 0; i < s_dedup_count; i++) {
        int idx = (s_dedup_head + i) % APP_CMD_DEDUP_MAX;
        if (strcmp(s_dedup[idx].command_id, command_id) == 0) {
            return idx;
        }
    }
    return -1;
}

/* 记录指令终态结果 */
static void dedup_record(const char *command_id, bool ack, const char *failure_code)
{
    int idx;
    if (s_dedup_count < APP_CMD_DEDUP_MAX) {
        idx = (s_dedup_head + s_dedup_count) % APP_CMD_DEDUP_MAX;
        s_dedup_count++;
    } else {
        idx = s_dedup_head;                     /* 覆盖最旧 */
        s_dedup_head = (s_dedup_head + 1) % APP_CMD_DEDUP_MAX;
    }
    strncpy(s_dedup[idx].command_id, command_id, sizeof(s_dedup[idx].command_id) - 1);
    s_dedup[idx].command_id[sizeof(s_dedup[idx].command_id) - 1] = '\0';
    s_dedup[idx].ack = ack;
    s_dedup[idx].failure_code[0] = '\0';
    if (!ack && failure_code) {
        strncpy(s_dedup[idx].failure_code, failure_code, sizeof(s_dedup[idx].failure_code) - 1);
        s_dedup[idx].failure_code[sizeof(s_dedup[idx].failure_code) - 1] = '\0';
    }
}

void app_command_set_imei(const char *imei)
{
    if (imei) {
        strncpy(g_imei, imei, sizeof(g_imei) - 1);
        g_imei[sizeof(g_imei) - 1] = '\0';
    }
}

/* 发送命令结果（使用静态缓冲区，不使用 cJSON malloc/free） */
void app_command_send_result(const char *command_id, const char *status,
                              const char *failure_code, const char *failed_reason)
{
    if (!command_id || !status) return;
    /* 静态缓冲区：避免栈上大缓冲区 + 避免 malloc
     * 此函数只在 main_task 上下文调用，静态缓冲区无并发风险 */
    static char s_json[512];
    int len = 0;
    if (app_protocol_build_command_result(g_imei, command_id, status,
                                          failure_code, failed_reason,
                                          s_json, sizeof(s_json), &len) == 0) {
        if (app_mqtt_is_connected()) {
            app_mqtt_publish_telemetry(s_json, len);
        }
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
static void dispatch(const app_rpc_parsed_t *rpc, const char *command_id)
{
    if (!rpc) return;
    const char *method = rpc->method;

    /* 幂等去重（硬件协议 4）：相同 command_id 重复到达时不重复执行，
     * 仅重发之前缓存的结果（message_id 由 command_id+status 决定，天然不变） */
    int prev = dedup_lookup(command_id);
    if (prev >= 0) {
        APP_LOGI("dup command %s, resend cached result", command_id);
        if (s_dedup[prev].ack) {
            app_command_send_result(command_id, APP_CMD_ACK, NULL, NULL);
        } else {
            app_command_send_result(command_id, APP_CMD_FAILED,
                                    s_dedup[prev].failure_code[0] ? s_dedup[prev].failure_code : NULL,
                                    "duplicate command, resend cached result");
        }
        return;
    }

    if (strcmp(method, "DEVICE_MODE") == 0) {
        /* 平台模式切换（硬件协议：searching/walking/supervise/lowpower/sleep） */
        if (rpc->mode[0] == '\0') {
            APP_LOGE("DEVICE_MODE missing mode param");
            app_command_send_result(command_id, APP_CMD_FAILED,
                                     "INVALID_PARAMETER", "mode param missing");
            dedup_record(command_id, false, "INVALID_PARAMETER");
            return;
        }
        app_mode_e m = app_mode_from_string(rpc->mode);
        if (m == APP_MODE_NUM) {
            APP_LOGE("DEVICE_MODE invalid mode: %s", rpc->mode);
            app_command_send_result(command_id, APP_CMD_FAILED,
                                     "UNSUPPORTED_PARAMETER", "unsupported mode value");
            dedup_record(command_id, false, "UNSUPPORTED_PARAMETER");
            return;
        }
        app_mode_set(m);
        app_command_send_result(command_id, APP_CMD_ACK, NULL, NULL);
        dedup_record(command_id, true, NULL);
    } else if (strcmp(method, "HIGH_FREQUENCY_LOCATION_START") == 0) {
        /* 休眠模式下：收到定位指令后做一次定位+上报，不切模式（需求 1 休眠模式） */
        if (app_mode_get() == APP_MODE_SLEEP) {
            APP_LOGI("sleep mode one-shot location");
            app_main_trigger_one_shot_location();
        } else {
            /* 高频定位 = 寻宠模式（需求 1：10 秒/次，10 分钟自动切回看护） */
            app_mode_set(APP_MODE_SEARCHING);
        }
        app_command_send_result(command_id, APP_CMD_ACK, NULL, NULL);
        dedup_record(command_id, true, NULL);
    } else if (strcmp(method, "HIGH_FREQUENCY_LOCATION_STOP") == 0) {
        /* 停止高频定位：切回看护模式（默认运行模式） */
        app_mode_set(APP_MODE_SUPERVISE);
        app_command_send_result(command_id, APP_CMD_ACK, NULL, NULL);
        dedup_record(command_id, true, NULL);
    } else if (strcmp(method, "SOUND") == 0) {
        /* 持续响铃：每秒响一次，默认持续 30 秒（硬件协议默认值）
         * duration_seconds = 0 视为停止指令（需求 4：持续时间内收到停止指令停止）
         * duration_seconds < 0 参数非法（硬件协议：必须是正整数） */
        if (rpc->duration_seconds < 0) {
            app_command_send_result(command_id, APP_CMD_FAILED,
                                     "INVALID_PARAMETER", "duration_seconds must be positive");
            dedup_record(command_id, false, "INVALID_PARAMETER");
            return;
        }
        int duration = APP_CMD_DEFAULT_DURATION_S;
        if (rpc->duration_seconds > 0) duration = rpc->duration_seconds;
        if (duration == 0) {
            bsp_buzzer_stop();
        } else {
            bsp_buzzer_beep_async((uint32_t)duration);
        }
        app_command_send_result(command_id, APP_CMD_ACK, NULL, NULL);
        dedup_record(command_id, true, NULL);
    } else if (strcmp(method, "SOUND_STOP") == 0) {
        bsp_buzzer_stop();
        app_command_send_result(command_id, APP_CMD_ACK, NULL, NULL);
        dedup_record(command_id, true, NULL);
    } else if (strcmp(method, "LIGHT") == 0) {
        /* 指示灯持续快闪（5Hz，需求 5），默认持续 30 秒（硬件协议默认值）
         * 参数规则同 SOUND */
        if (rpc->duration_seconds < 0) {
            app_command_send_result(command_id, APP_CMD_FAILED,
                                     "INVALID_PARAMETER", "duration_seconds must be positive");
            dedup_record(command_id, false, "INVALID_PARAMETER");
            return;
        }
        int duration = APP_CMD_DEFAULT_DURATION_S;
        if (rpc->duration_seconds > 0) duration = rpc->duration_seconds;
        if (duration == 0) {
            bsp_led_stop();
        } else {
            bsp_led_flash_async((uint32_t)duration);
        }
        app_command_send_result(command_id, APP_CMD_ACK, NULL, NULL);
        dedup_record(command_id, true, NULL);
    } else if (strcmp(method, "LIGHT_STOP") == 0) {
        bsp_led_stop();
        app_command_send_result(command_id, APP_CMD_ACK, NULL, NULL);
        dedup_record(command_id, true, NULL);
    } else if (strcmp(method, "LOCATION_FREQUENCY") == 0) {
        /* 调整常规定位频率（平台协议 6）：interval_seconds 生效，
         * 覆盖当前模式默认周期（休眠模式除外） */
        if (rpc->interval_seconds <= 0) {
            app_command_send_result(command_id, APP_CMD_FAILED,
                                     "INVALID_PARAMETER", "interval_seconds must be positive");
            dedup_record(command_id, false, "INVALID_PARAMETER");
            return;
        }
        app_mode_set_platform_interval(rpc->interval_seconds);
        app_command_send_result(command_id, APP_CMD_ACK, NULL, NULL);
        dedup_record(command_id, true, NULL);
    } else if (strcmp(method, "SHUTDOWN") == 0) {
        /* 平台关机指令（需求 V1.8：软件关机模式已删除）：
         * 先发 ACK，再由 custom_main 上报 OFFLINE → 断 MQTT →
         * cm_pm_poweroff() 断电关机（与长按 PWR_ON/OFF 硬件关机殊途同归） */
        app_command_send_result(command_id, APP_CMD_ACK, NULL, NULL);
        dedup_record(command_id, true, NULL);
        bsp_buzzer_stop();
        bsp_led_stop();
        osDelay(APP_MS_TO_TICK(500)); /* 等待 ACK telemetry 发出 */
        app_main_execute_poweroff();
    } else if (strcmp(method, "GET_VERSION") == 0) {
        /* 平台版本查询（需求 6.7）：应答当前 app_ver/hw_ver/modem_ver 等 */
        app_main_report_device_info();
        app_command_send_result(command_id, APP_CMD_ACK, NULL, NULL);
        dedup_record(command_id, true, NULL);
    } else if (strcmp(method, "OTA") == 0) {
        const char *url = rpc->url;
        if (!url || url[0] == '\0') {
            APP_LOGE("ota cmd missing url");
            app_command_send_result(command_id, APP_CMD_FAILED,
                                     "INVALID_PARAMETER", "ota url missing");
            dedup_record(command_id, false, "INVALID_PARAMETER");
            return;
        }
        if (app_ota_is_running()) {
            APP_LOGW("ota already running");
            app_command_send_result(command_id, APP_CMD_FAILED,
                                     "DEVICE_BUSY", "ota already running");
            dedup_record(command_id, false, "DEVICE_BUSY");
            return;
        }
        int r = app_ota_start(url, ota_progress_cb);
        if (r == 0) {
            APP_LOGI("ota started: %s", url);
            app_command_send_result(command_id, APP_CMD_ACK, NULL, NULL);
            dedup_record(command_id, true, NULL);
        } else {
            APP_LOGE("ota start fail:%d", r);
            app_command_send_result(command_id, APP_CMD_FAILED,
                                     "INTERNAL_ERROR", "ota task create fail");
            dedup_record(command_id, false, "INTERNAL_ERROR");
        }
    } else {
        APP_LOGW("unknown method: %s", method);
        app_command_send_result(command_id, APP_CMD_FAILED,
                                 "UNSUPPORTED_COMMAND", "unknown method");
        dedup_record(command_id, false, "UNSUPPORTED_COMMAND");
    }
}

void app_command_handle(const char *topic, const char *payload, int payload_len)
{
    if (!topic || !payload) return;

    app_rpc_parsed_t rpc;
    if (app_protocol_parse_rpc(payload, payload_len, &rpc) != 0) {
        APP_LOGE("rpc parse fail");
        return;
    }

    /* 硬件协议 1：command_id 必填非空；无效时不执行动作 */
    if (rpc.command_id[0] == '\0') {
        APP_LOGW("rpc without command_id, ignore (method=%s)", rpc.method);
        return;
    }
    const char *command_id = rpc.command_id;
    APP_LOGI("rpc method=%s cmd_id=%s", rpc.method, command_id);

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
    dispatch(&rpc, command_id);
}
