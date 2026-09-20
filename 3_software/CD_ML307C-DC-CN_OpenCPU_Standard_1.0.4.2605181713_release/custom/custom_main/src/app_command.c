/**
 * @file    app_command.c
 * @brief   云端指令处理（协议 4.1：SOUND/LIGHT/DEVICE_MODE/GET_STATE）
 *          - DEVICE_MODE：平台模式切换（需求 1 / 硬件协议 DEVICE_MODE）
 *          - SOUND/LIGHT 异步持续 duration_seconds，默认 30 秒（硬件协议）
 *          - GET_STATE：应答完整设备状态 state 事件（协议 4.1）；休眠模式
 *            下额外触发单次定位上报（协议 4.2）
 *          - 按 command_id 幂等去重（硬件协议 4：重复指令仅重发缓存结果）
 *          - 失败码符合硬件协议 5：INVALID_PARAMETER/DEVICE_BUSY/INTERNAL_ERROR
 *          - RPC method=OTA 已废弃（联调协议 V1 2.2：OTA 由属性快照驱动），
 *            收到该指令统一回 UNSUPPORTED_COMMAND
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
#include "bsp.h"

/* 由 custom_main.c 提供的实现声明 */
extern void app_main_trigger_one_shot_location(void);
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
    } else if (strcmp(method, "SOUND") == 0) {
        /* 持续响铃：每秒响一次（协议 4.1）：
         * duration_seconds 未提供→默认 30 秒；显式 0→立即停止；负数→INVALID_PARAMETER */
        if (rpc->duration_seconds_present && rpc->duration_seconds < 0) {
            app_command_send_result(command_id, APP_CMD_FAILED,
                                     "INVALID_PARAMETER", "duration_seconds must be positive");
            dedup_record(command_id, false, "INVALID_PARAMETER");
            return;
        }
        if (rpc->duration_seconds_present && rpc->duration_seconds == 0) {
            bsp_buzzer_stop();
        } else {
            uint32_t duration = rpc->duration_seconds_present
                                ? (uint32_t)rpc->duration_seconds
                                : (uint32_t)APP_CMD_DEFAULT_DURATION_S;
            bsp_buzzer_beep_async(duration);
        }
        app_command_send_result(command_id, APP_CMD_ACK, NULL, NULL);
        dedup_record(command_id, true, NULL);
    } else if (strcmp(method, "LIGHT") == 0) {
        /* 指示灯持续快闪（5Hz，需求 5），参数规则同 SOUND（协议 4.1） */
        if (rpc->duration_seconds_present && rpc->duration_seconds < 0) {
            app_command_send_result(command_id, APP_CMD_FAILED,
                                     "INVALID_PARAMETER", "duration_seconds must be positive");
            dedup_record(command_id, false, "INVALID_PARAMETER");
            return;
        }
        if (rpc->duration_seconds_present && rpc->duration_seconds == 0) {
            bsp_led_stop();
        } else {
            uint32_t duration = rpc->duration_seconds_present
                                ? (uint32_t)rpc->duration_seconds
                                : (uint32_t)APP_CMD_DEFAULT_DURATION_S;
            bsp_led_flash_async(duration);
        }
        app_command_send_result(command_id, APP_CMD_ACK, NULL, NULL);
        dedup_record(command_id, true, NULL);
    } else if (strcmp(method, "GET_STATE") == 0) {
        /* 平台状态查询（协议 4.1）：应答完整设备状态 state 事件；
         * 休眠模式下额外触发一次单次定位+上报（协议 4.2） */
        app_main_report_device_info();
        if (app_mode_get() == APP_MODE_SLEEP) {
            app_main_trigger_one_shot_location();
        }
        app_command_send_result(command_id, APP_CMD_ACK, NULL, NULL);
        dedup_record(command_id, true, NULL);
    } else if (strcmp(method, "OTA") == 0) {
        /* 联调协议 V1 2.2：OTA 由属性快照（fw_* 共享属性）驱动，
         * RPC method=OTA 废弃，统一拒绝 */
        APP_LOGW("rpc OTA deprecated, use fw_* shared attributes");
        app_command_send_result(command_id, APP_CMD_FAILED,
                                 "UNSUPPORTED_COMMAND",
                                 "ota via fw_* shared attributes");
        dedup_record(command_id, false, "UNSUPPORTED_COMMAND");
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
