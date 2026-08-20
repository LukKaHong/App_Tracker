/**
 * @file    app_protocol.c
 * @brief   平台协议 JSON 组装与 RPC 解析
 *          全部使用静态/栈缓冲区 + snprintf/manual parsing，
 *          不使用 cJSON malloc/free，避免与 cmmqtt-m 任务并发堆损坏
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "cm_sys.h"
#include "cm_rtc.h"
#include "app_protocol.h"
#include "app_utils.h"
#include "app_config.h"
#include "app_log.h"

/* ===== 手动 JSON 字段提取（不使用 malloc）===== */

/* 在 json 中查找 "key":"value"，将 value 拷贝到 out
 * 处理基本转义（\" \\ \n \t \r）
 * 返回 true 找到并拷贝，false 未找到 */
static bool json_get_string(const char *json, const char *key,
                             char *out, size_t out_len)
{
    if (!json || !key || !out || out_len == 0) return false;
    char pattern[48];
    int n = snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    if (n <= 0 || n >= (int)sizeof(pattern)) return false;
    const char *p = strstr(json, pattern);
    if (!p) return false;
    p += n;
    /* 跳过 : 和空白 */
    while (*p == ':' || *p == ' ' || *p == '\t') p++;
    if (*p != '"') return false;
    p++; /* 跳过开引号 */
    size_t i = 0;
    while (*p && *p != '"' && i < out_len - 1) {
        if (*p == '\\' && p[1]) {
            p++;
            switch (*p) {
                case 'n':  out[i++] = '\n'; break;
                case 't':  out[i++] = '\t'; break;
                case 'r':  out[i++] = '\r'; break;
                case '"':  out[i++] = '"';  break;
                case '\\': out[i++] = '\\'; break;
                default:   out[i++] = *p;   break;
            }
        } else {
            out[i++] = *p;
        }
        p++;
    }
    out[i] = '\0';
    return true;
}

/* 在 json 中查找 "key":<number>，将数值写入 out
 * 返回 true 找到，false 未找到或非数值 */
static bool json_get_int(const char *json, const char *key, int *out)
{
    if (!json || !key || !out) return false;
    char pattern[48];
    int n = snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    if (n <= 0 || n >= (int)sizeof(pattern)) return false;
    const char *p = strstr(json, pattern);
    if (!p) return false;
    p += n;
    while (*p == ':' || *p == ' ' || *p == '\t') p++;
    if (*p == '"') return false; /* 是字符串而非数值 */
    *out = atoi(p);
    return true;
}

/* ===== 组装命令结果 JSON 到 caller-provided 缓冲区 ===== */
int app_protocol_build_command_result(const char *imei, const char *command_id,
                                       const char *status,
                                       const char *failure_code,
                                       const char *failed_reason,
                                       char *out_buf, size_t buf_size, int *out_len)
{
    if (!imei || !command_id || !status || !out_buf || buf_size == 0) return -1;

    char ts[24];
    app_util_format_rfc3339(cm_rtc_get_current_time(), ts, sizeof(ts));

    /* failure_code / failed_reason 可能为 NULL，需条件拼接 */
    int len;
    if (failure_code && failed_reason) {
        len = snprintf(out_buf, buf_size,
            "{\"event_type\":\"%s\""
            ",\"message_id\":\"cmd_result_%s_%s\""
            ",\"command_id\":\"%s\""
            ",\"status\":\"%s\""
            ",\"event_time\":\"%s\""
            ",\"failure_code\":\"%s\""
            ",\"failed_reason\":\"%s\"}",
            APP_EVT_COMMAND_RESULT, command_id, status,
            command_id, status, ts,
            failure_code, failed_reason);
    } else if (failure_code) {
        len = snprintf(out_buf, buf_size,
            "{\"event_type\":\"%s\""
            ",\"message_id\":\"cmd_result_%s_%s\""
            ",\"command_id\":\"%s\""
            ",\"status\":\"%s\""
            ",\"event_time\":\"%s\""
            ",\"failure_code\":\"%s\"}",
            APP_EVT_COMMAND_RESULT, command_id, status,
            command_id, status, ts,
            failure_code);
    } else {
        len = snprintf(out_buf, buf_size,
            "{\"event_type\":\"%s\""
            ",\"message_id\":\"cmd_result_%s_%s\""
            ",\"command_id\":\"%s\""
            ",\"status\":\"%s\""
            ",\"event_time\":\"%s\"}",
            APP_EVT_COMMAND_RESULT, command_id, status,
            command_id, status, ts);
    }

    if (len <= 0 || len >= (int)buf_size) {
        APP_LOGE("cmd_result json overflow len=%d", len);
        return -2;
    }
    if (out_len) *out_len = len;
    return 0;
}

/* ===== 解析 RPC payload（不使用 cJSON malloc/free）===== */
int app_protocol_parse_rpc(const char *payload, int payload_len,
                            app_rpc_parsed_t *out)
{
    if (!payload || !out) return -1;

    /* 需要一个 null 终止的副本；payload 可能不带 '\0' */
    char buf[1024];
    if (payload_len < 0) payload_len = (int)strlen(payload);
    if (payload_len >= (int)sizeof(buf)) {
        APP_LOGE("rpc payload too long %d", payload_len);
        return -2;
    }
    memcpy(buf, payload, payload_len);
    buf[payload_len] = '\0';

    memset(out, 0, sizeof(*out));
    out->duration_seconds = -1;
    out->interval_seconds = -1;
    out->mode[0] = '\0';
    out->url[0] = '\0';

    /* 提取 method */
    if (!json_get_string(buf, "method", out->method, sizeof(out->method))) {
        APP_LOGE("rpc: method not found");
        return -3;
    }

    /* 提取 params 中的字段（json_get_string 使用 strstr 全局搜索，
     * 因为这些 key 名在 RPC payload 中只出现一次） */
    json_get_string(buf, "command_id", out->command_id, sizeof(out->command_id));

    int tmp;
    if (json_get_int(buf, "duration_seconds", &tmp)) {
        out->duration_seconds = tmp;
    }
    if (json_get_int(buf, "interval_seconds", &tmp)) {
        out->interval_seconds = tmp;
    }
    json_get_string(buf, "mode", out->mode, sizeof(out->mode));
    json_get_string(buf, "url", out->url, sizeof(out->url));

    return 0;
}

/* 从 RPC topic 提取 requestId：v1/devices/me/rpc/request/{id} */
int app_protocol_extract_request_id(const char *topic, char *out, size_t out_len)
{
    if (!topic) return -1;
    const char *p = strrchr(topic, '/');
    if (!p) return -2;
    p++;
    if (out_len <= strlen(p)) return -3;
    strcpy(out, p);
    return 0;
}
