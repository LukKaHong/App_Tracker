/**
 * @file    app_provisioning.c
 * @brief   HTTP POST /api/device/v1/provision 实现
 *          - 拼接 canonical string
 *          - HMAC-SHA256 签名（mbedtls）
 *          - 同步 HTTP POST
 *          - 解析响应中的 MQTT 凭证
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "cm_sys.h"
#include "cm_rtc.h"
#include "cm_http.h"
#include "cm_os.h"
#include "cJSON.h"
#include "app_log.h"
#include "app_config.h"
#include "app_utils.h"
#include "app_storage.h"
#include "app_provisioning.h"

/* 从 URL 中提取 host:port 和 path */
static int parse_url(const char *url, char *host, size_t host_len,
                     uint16_t *port, bool *use_https)
{
    const char *p = url;
    if (strncmp(p, "https://", 8) == 0) {
        *use_https = true;
        *port = 443;
        p += 8;
    } else if (strncmp(p, "http://", 7) == 0) {
        *use_https = false;
        *port = 80;
        p += 7;
    } else {
        return -1;
    }
    /* 复制 host 直到 ':' '/' 或结束 */
    size_t i = 0;
    while (*p && *p != ':' && *p != '/' && i + 1 < host_len) {
        host[i++] = *p++;
    }
    host[i] = '\0';
    if (*p == ':') {
        *port = (uint16_t)strtol(p + 1, NULL, 10);
    }
    return 0;
}

static app_prov_result_e map_error_code(const char *code)
{
    if (!code) return APP_PROV_ERR_UNKNOWN;
    if (strcmp(code, "INVALID_DEVICE_SIGNATURE") == 0) return APP_PROV_ERR_SIGNATURE;
    if (strcmp(code, "DEVICE_NOT_FOUND") == 0)        return APP_PROV_ERR_NOT_FOUND;
    if (strcmp(code, "DEVICE_NONCE_REPLAY") == 0)     return APP_PROV_ERR_UNKNOWN;
    if (strcmp(code, "DEVICE_MODEL_MISMATCH") == 0)   return APP_PROV_ERR_UNKNOWN;
    if (strcmp(code, "DEVICE_NOT_IN_TENANT") == 0)    return APP_PROV_ERR_NOT_IN_TENANT;
    if (strcmp(code, "DEVICE_FROZEN") == 0)           return APP_PROV_ERR_FROZEN;
    if (strcmp(code, "DEVICE_VOIDED") == 0)           return APP_PROV_ERR_FROZEN;
    return APP_PROV_ERR_UNKNOWN;
}

app_prov_result_e app_provisioning_request(app_mqtt_credential_t *cred)
{
    if (!cred) return APP_PROV_ERR_UNKNOWN;

    char imei[16] = {0};
    if (cm_sys_get_imei(imei) != 0) {
        return APP_PROV_ERR_UNKNOWN;
    }

    /* 1. 组装 canonical */
    char ts[24];
    app_util_format_rfc3339(cm_rtc_get_current_time(), ts, sizeof(ts));
    char nonce[20];
    app_util_gen_nonce(nonce, sizeof(nonce));

    char canonical[512];
    snprintf(canonical, sizeof(canonical),
             "imei=%s\n"
             "device_sn=%s\n"
             "model_code=%s\n"
             "firmware_version=%s\n"
             "protocol_version=%s\n"
             "timestamp=%s\n"
             "nonce=%s",
             imei, imei, APP_MODEL_CODE,
             APP_FIRMWARE_VERSION, APP_PROTOCOL_VERSION, ts, nonce);

    /* 2. 签名 */
    char sig_hex[65];
    if (app_util_hmac_sha256_hex(APP_PROVISIONING_SECRET, strlen(APP_PROVISIONING_SECRET),
                                  canonical, strlen(canonical),
                                  sig_hex, sizeof(sig_hex)) != 0) {
        APP_LOGE("hmac fail");
        return APP_PROV_ERR_UNKNOWN;
    }

    /* 3. 构造 JSON body */
    cJSON *body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "imei", imei);
    cJSON_AddStringToObject(body, "device_sn", imei);
    cJSON_AddStringToObject(body, "model_code", APP_MODEL_CODE);
    cJSON_AddStringToObject(body, "firmware_version", APP_FIRMWARE_VERSION);
    cJSON_AddStringToObject(body, "protocol_version", APP_PROTOCOL_VERSION);
    cJSON_AddStringToObject(body, "timestamp", ts);
    cJSON_AddStringToObject(body, "nonce", nonce);
    cJSON_AddStringToObject(body, "signature", sig_hex);
    char *body_str = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);
    if (!body_str) return APP_PROV_ERR_UNKNOWN;

    APP_LOGI("prov body len=%d", (int)strlen(body_str));

    /* 4. HTTP POST 同步 */
    char url[256];
    snprintf(url, sizeof(url), "%s%s", APP_SAAS_BASE_URL, APP_PROVISIONING_PATH);
    bool use_https = false;
    uint16_t port = 80;
    char host[128] = {0};
    if (parse_url(url, host, sizeof(host), &port, &use_https) != 0) {
        free(body_str);
        return APP_PROV_ERR_UNKNOWN;
    }
    APP_LOGI("prov http %s:%u %s", host, port, use_https ? "(tls)" : "(plain)");

    char url_with_port[256];
    snprintf(url_with_port, sizeof(url_with_port), "%s://%s:%u",
             use_https ? "https" : "http", host, port);

    cm_httpclient_handle_t handle = NULL;
    cm_httpclient_ret_code_e rc = cm_httpclient_create((const uint8_t *)url_with_port, NULL, &handle);
    if (rc != CM_HTTP_RET_CODE_OK || !handle) {
        APP_LOGE("http create fail:%d", rc);
        free(body_str);
        return APP_PROV_ERR_NETWORK;
    }

    cm_httpclient_cfg_t cfg = {0};
    cfg.ssl_enable = use_https ? 1 : 0;
    cfg.conn_timeout = 30;
    cfg.rsp_timeout = 30;
    cm_httpclient_set_cfg(handle, cfg);

    /* 设置 Content-Type 头 */
    const char *hdr = "Content-Type: application/json\r\n";
    cm_httpclient_custom_header_set(handle, (uint8_t *)hdr, (uint16_t)strlen(hdr));

    cm_httpclient_sync_param_t param = {0};
    param.method = HTTPCLIENT_REQUEST_POST;
    param.path = (const uint8_t *)APP_PROVISIONING_PATH;
    param.content_length = (uint32_t)strlen(body_str);
    param.content = (uint8_t *)body_str;

    cm_httpclient_sync_response_t resp = {0};
    rc = cm_httpclient_sync_request(handle, param, &resp);

    app_prov_result_e result = APP_PROV_ERR_NETWORK;
    if (rc == CM_HTTP_RET_CODE_OK) {
        APP_LOGI("http resp=%u len=%u", resp.response_code, resp.response_content_len);
        if (resp.response_content && resp.response_content_len > 0) {
            /* 防止非 \0 结尾 */
            char *resp_str = (char *)malloc(resp.response_content_len + 1);
            if (resp_str) {
                memcpy(resp_str, resp.response_content, resp.response_content_len);
                resp_str[resp.response_content_len] = '\0';
                cJSON *root = cJSON_Parse(resp_str);
                if (root) {
                    cJSON *err = cJSON_GetObjectItem(root, "error");
                    if (err) {
                        cJSON *code = cJSON_GetObjectItem(err, "code");
                        result = map_error_code(code ? code->valuestring : NULL);
                        APP_LOGE("prov err code=%s",
                                 code ? code->valuestring : "(null)");
                    } else {
                        cJSON *data = cJSON_GetObjectItem(root, "data");
                        if (data) {
                            cJSON *h  = cJSON_GetObjectItem(data, "mqtt_host");
                            cJSON *p  = cJSON_GetObjectItem(data, "mqtt_port");
                            cJSON *ci = cJSON_GetObjectItem(data, "client_id");
                            cJSON *u  = cJSON_GetObjectItem(data, "username");
                            cJSON *pw = cJSON_GetObjectItem(data, "password");
                            cJSON *ct = cJSON_GetObjectItem(data, "credential_type");
                            cJSON *ia = cJSON_GetObjectItem(data, "issued_at");
                            if (h && h->valuestring) strncpy(cred->mqtt_host, h->valuestring, sizeof(cred->mqtt_host) - 1);
                            if (p) cred->mqtt_port = (uint16_t)p->valuedouble;
                            if (ci && ci->valuestring) strncpy(cred->client_id, ci->valuestring, sizeof(cred->client_id) - 1);
                            if (u && u->valuestring) strncpy(cred->username, u->valuestring, sizeof(cred->username) - 1);
                            if (pw && pw->valuestring) strncpy(cred->password, pw->valuestring, sizeof(cred->password) - 1);
                            if (ct && ct->valuestring) strncpy(cred->credential_type, ct->valuestring, sizeof(cred->credential_type) - 1);
                            if (ia && ia->valuestring) strncpy(cred->issued_at, ia->valuestring, sizeof(cred->issued_at) - 1);
                            result = APP_PROV_OK;
                            APP_LOGI("prov ok mqtt_host=%s port=%u", cred->mqtt_host, cred->mqtt_port);
                        }
                    }
                    cJSON_Delete(root);
                }
                free(resp_str);
            }
        }
    } else {
        APP_LOGE("http req fail:%d", rc);
    }

    cm_httpclient_sync_free_data(handle);
    cm_httpclient_delete(handle);
    free(body_str);
    return result;
}
