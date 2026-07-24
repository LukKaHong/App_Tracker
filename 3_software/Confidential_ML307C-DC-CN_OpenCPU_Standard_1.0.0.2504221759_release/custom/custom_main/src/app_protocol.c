/**
 * @file    app_protocol.c
 * @brief   平台协议 JSON 组装与 RPC 解析
 *          使用 cJSON（third-party/cJSONFiles/cJSON）
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "cJSON.h"
#include "cm_sys.h"
#include "cm_rtc.h"
#include "app_protocol.h"
#include "app_utils.h"
#include "app_config.h"
#include "app_log.h"

/* 拼接 URL safe 字符串 */
static void safe_copy(char *dst, size_t dst_len, const char *src)
{
    if (!src) { if (dst_len > 0) dst[0] = '\0'; return; }
    strncpy(dst, src, dst_len - 1);
    dst[dst_len - 1] = '\0';
}

/* 组装定位事件 telemetry JSON
 * 返回 cJSON 对象指针，调用者需 cJSON_Delete */
static cJSON *build_loc_event(const char *imei, const char *boot_id,
                               uint32_t seq, const app_location_t *loc,
                               int battery_level, int signal_strength,
                               bool is_offline)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "event_type", APP_EVT_LOCATION);

    char msgid[APP_MSG_ID_MAX_LEN];
    app_util_gen_loc_message_id(imei, boot_id, seq, msgid, sizeof(msgid));
    cJSON_AddStringToObject(root, "message_id", msgid);

    cJSON_AddStringToObject(root, "imei", imei);
    cJSON_AddStringToObject(root, "device_sn", imei);

    char ts[24];
    app_util_format_rfc3339(cm_rtc_get_current_time(), ts, sizeof(ts));
    cJSON_AddStringToObject(root, "event_time", ts);

    cJSON_AddNumberToObject(root, "longitude", loc->longitude);
    cJSON_AddNumberToObject(root, "latitude", loc->latitude);
    cJSON_AddStringToObject(root, "coordinate_system",
                             loc->coord_sys[0] ? loc->coord_sys : "WGS84");
    cJSON_AddNumberToObject(root, "accuracy", loc->accuracy);
    cJSON_AddStringToObject(root, "source",
                             loc->source[0] ? loc->source : "GPS");

    if (battery_level >= 0) cJSON_AddNumberToObject(root, "battery_level", battery_level);
    cJSON_AddStringToObject(root, "network_type", "LTE");
    if (signal_strength != 0) cJSON_AddNumberToObject(root, "signal_strength", signal_strength);
    cJSON_AddNumberToObject(root, "speed", loc->speed);
    cJSON_AddNumberToObject(root, "heading", loc->heading);
    cJSON_AddNumberToObject(root, "altitude", loc->altitude);
    cJSON_AddStringToObject(root, "boot_id", boot_id);
    cJSON_AddNumberToObject(root, "sequence_no", (double)seq);
    cJSON_AddBoolToObject(root, "is_offline_upload", is_offline);
    return root;
}

/* 组装定位事件为字符串（用于 publish / 缓存）
 * 返回 0 成功，buf 为 cJSON_PrintUnformatted 分配，需 free */
int app_protocol_build_location(const char *imei, const char *boot_id,
                                 uint32_t seq, const app_location_t *loc,
                                 int battery_level, int signal_strength,
                                 bool is_offline,
                                 char **out_json, int *out_len,
                                 char *out_msgid, size_t msgid_len,
                                 char *out_event_time, size_t et_len)
{
    if (!imei || !boot_id || !loc || !out_json) return -1;
    cJSON *root = build_loc_event(imei, boot_id, seq, loc,
                                   battery_level, signal_strength, is_offline);
    if (!root) return -2;

    /* 提取 message_id / event_time 给上层缓存使用 */
    cJSON *m = cJSON_GetObjectItem(root, "message_id");
    if (m && m->valuestring && out_msgid) {
        safe_copy(out_msgid, msgid_len, m->valuestring);
    }
    cJSON *t = cJSON_GetObjectItem(root, "event_time");
    if (t && t->valuestring && out_event_time) {
        safe_copy(out_event_time, et_len, t->valuestring);
    }

    char *s = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!s) return -3;
    *out_json = s;
    if (out_len) *out_len = (int)strlen(s);
    return 0;
}

/* 组装状态事件 */
int app_protocol_build_state(const char *imei, const char *online_status,
                              int battery_level, const char *firmware_version,
                              int signal_strength, int charging_status,
                              char **out_json, int *out_len)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "event_type", APP_EVT_STATE);

    char ts[24];
    app_util_format_rfc3339(cm_rtc_get_current_time(), ts, sizeof(ts));

    char msgid[APP_MSG_ID_MAX_LEN];
    snprintf(msgid, sizeof(msgid), "state_%s_%s", imei, ts);
    cJSON_AddStringToObject(root, "message_id", msgid);

    cJSON_AddStringToObject(root, "imei", imei);
    cJSON_AddStringToObject(root, "device_sn", imei);
    cJSON_AddStringToObject(root, "online_status", online_status);
    cJSON_AddStringToObject(root, "event_time", ts);
    if (battery_level >= 0) cJSON_AddNumberToObject(root, "battery_level", battery_level);
    if (firmware_version) cJSON_AddStringToObject(root, "firmware_version", firmware_version);
    cJSON_AddStringToObject(root, "network_type", "LTE");
    if (signal_strength != 0) cJSON_AddNumberToObject(root, "signal_strength", signal_strength);
    /* 充电状态：0=放电 1=充电中 2=充满 */
    cJSON_AddNumberToObject(root, "charging_status", charging_status);

    char *s = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!s) return -1;
    *out_json = s;
    if (out_len) *out_len = (int)strlen(s);
    return 0;
}

/* 组装命令结果事件 */
int app_protocol_build_command_result(const char *imei, const char *command_id,
                                       const char *status,
                                       const char *failure_code,
                                       const char *failed_reason,
                                       char **out_json, int *out_len)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "event_type", APP_EVT_COMMAND_RESULT);

    char ts[24];
    app_util_format_rfc3339(cm_rtc_get_current_time(), ts, sizeof(ts));

    char msgid[APP_MSG_ID_MAX_LEN];
    snprintf(msgid, sizeof(msgid), "cmd_result_%s_%s", command_id, status);
    cJSON_AddStringToObject(root, "message_id", msgid);

    cJSON_AddStringToObject(root, "command_id", command_id);
    cJSON_AddStringToObject(root, "status", status);
    cJSON_AddStringToObject(root, "event_time", ts);
    if (failure_code) cJSON_AddStringToObject(root, "failure_code", failure_code);
    if (failed_reason) cJSON_AddStringToObject(root, "failed_reason", failed_reason);

    char *s = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!s) return -1;
    *out_json = s;
    if (out_len) *out_len = (int)strlen(s);
    return 0;
}

/* 解析 RPC payload，返回 method 字符串和 command_id（需调用方 free *out_method）
 * out_params 为 cJSON 对象指针，调用方负责 cJSON_Delete */
int app_protocol_parse_rpc(const char *payload, int payload_len,
                            char **out_method, char *out_command_id, size_t cmd_id_len,
                            cJSON **out_params)
{
    if (!payload) return -1;
    char *buf = (char *)malloc(payload_len + 1);
    if (!buf) return -2;
    memcpy(buf, payload, payload_len);
    buf[payload_len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) return -3;

    cJSON *m = cJSON_GetObjectItem(root, "method");
    if (!m || !m->valuestring) {
        cJSON_Delete(root);
        return -4;
    }
    *out_method = strdup(m->valuestring);

    cJSON *p = cJSON_GetObjectItem(root, "params");
    if (p) {
        p = cJSON_Duplicate(p, 1);
        cJSON *cid = cJSON_GetObjectItem(root, "params");
        if (cid) {
            cJSON *cid_item = cJSON_GetObjectItem(cid, "command_id");
            if (cid_item && cid_item->valuestring && out_command_id) {
                safe_copy(out_command_id, cmd_id_len, cid_item->valuestring);
            }
        }
    }
    cJSON_Delete(root);
    if (out_params) *out_params = p;
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
