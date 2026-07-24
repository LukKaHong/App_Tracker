/**
 * @file    app_ota.c
 * @brief   全系统差分升级：HTTP 异步下载 + cm_ota_* 写入 + 触发升级
 *          - 通过 HTTP 异步接口分块下载，避免大包占用内存
 *          - 写入完成后调用 cm_ota_upgrade() 触发重启
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "cm_os.h"
#include "cm_http.h"
#include "cm_ota.h"
#include "app_log.h"
#include "app_config.h"
#include "app_ota.h"

typedef struct {
    char    url[256];
    app_ota_progress_cb_t cb;
    bool    running;
    int     total_size;
    int     written;
} app_ota_ctx_t;

static app_ota_ctx_t s_ctx = {0};

bool app_ota_is_running(void) { return s_ctx.running; }

/* HTTP 异步回调 */
static void http_cb(cm_httpclient_handle_t h, cm_httpclient_callback_event_e evt, void *param)
{
    (void)h;
    switch (evt) {
    case CM_HTTP_CALLBACK_EVENT_RSP_HEADER_IND: {
        cm_httpclient_callback_rsp_header_param_t *hd =
            (cm_httpclient_callback_rsp_header_param_t *)param;
        APP_LOGI("ota http resp code=%u", hd->response_code);
        break;
    }
    case CM_HTTP_CALLBACK_EVENT_RSP_CONTENT_IND: {
        cm_httpclient_callback_rsp_content_param_t *ct =
            (cm_httpclient_callback_rsp_content_param_t *)param;
        if (ct->current_len > 0 && ct->response_content) {
            int w = cm_ota_firmware_write((const char *)ct->response_content, ct->current_len);
            if (w != 0) {
                APP_LOGE("ota write fail:%d", w);
            }
            s_ctx.written += ct->current_len;
            if (s_ctx.cb) {
                int percent = s_ctx.total_size > 0 ? (s_ctx.written * 100 / s_ctx.total_size) : 0;
                s_ctx.cb(APP_OTA_STATE_WRITING, percent);
            }
        }
        break;
    }
    case CM_HTTP_CALLBACK_EVENT_RSP_END_IND:
        APP_LOGI("ota download done, written=%d", s_ctx.written);
        if (s_ctx.cb) s_ctx.cb(APP_OTA_STATE_DONE, 100);
        cm_ota_upgrade();
        s_ctx.running = false;
        break;
    case CM_HTTP_CALLBACK_EVENT_ERROR_IND:
        APP_LOGE("ota http err:%d", (int)(intptr_t)param);
        if (s_ctx.cb) s_ctx.cb(APP_OTA_STATE_FAILED, 0);
        s_ctx.running = false;
        break;
    default:
        break;
    }
}

static void ota_task(void *arg)
{
    (void)arg;
    char url[256];
    strncpy(url, s_ctx.url, sizeof(url) - 1);
    url[sizeof(url) - 1] = '\0';

    /* 解析 URL 取 host/port/path */
    const char *p = url;
    bool use_https = false;
    uint16_t port = 80;
    if (strncmp(p, "https://", 8) == 0) { use_https = true; port = 443; p += 8; }
    else if (strncmp(p, "http://", 7) == 0) { use_https = false; port = 80; p += 7; }

    char host[128] = {0};
    const char *path = "/";
    size_t i = 0;
    while (*p && *p != ':' && *p != '/' && i + 1 < sizeof(host)) {
        host[i++] = *p++;
    }
    host[i] = '\0';
    if (*p == ':') {
        port = (uint16_t)strtol(p + 1, NULL, 10);
        while (*p && *p != '/') p++;
    }
    if (*p == '/') path = p;

    char url_with_port[256];
    snprintf(url_with_port, sizeof(url_with_port), "%s://%s:%u",
             use_https ? "https" : "http", host, port);

    cm_httpclient_handle_t handle = NULL;
    if (cm_httpclient_create((const uint8_t *)url_with_port, http_cb, &handle) != CM_HTTP_RET_CODE_OK) {
        APP_LOGE("ota http create fail");
        s_ctx.running = false;
        return;
    }
    cm_httpclient_cfg_t cfg = {0};
    cfg.ssl_enable = use_https ? 1 : 0;
    cfg.conn_timeout = 60;
    cfg.rsp_timeout = 60;
    cm_httpclient_set_cfg(handle, cfg);

    if (cm_ota_init() != 0) {
        APP_LOGE("ota init fail");
        s_ctx.running = false;
        return;
    }
    cm_ota_firmware_erase();

    /* 用同步接口做简化（适合小差分包）；
     * 大文件场景应改为异步 cm_httpclient_request_start + send 多次写入 */
    cm_httpclient_sync_param_t param = {0};
    param.method = HTTPCLIENT_REQUEST_GET;
    param.path = (const uint8_t *)path;
    cm_httpclient_sync_response_t resp = {0};
    if (cm_httpclient_sync_request(handle, param, &resp) == CM_HTTP_RET_CODE_OK) {
        APP_LOGI("ota http ok resp=%u len=%u",
                 resp.response_code, resp.response_content_len);
        if (resp.response_code == 200 && resp.response_content_len > 0) {
            cm_ota_set_otasize((int)resp.response_content_len);
            s_ctx.total_size = (int)resp.response_content_len;
            s_ctx.written = 0;
            if (s_ctx.cb) s_ctx.cb(APP_OTA_STATE_DOWNLOADING, 0);
            int w = cm_ota_firmware_write((const char *)resp.response_content,
                                           resp.response_content_len);
            if (w == 0) {
                if (s_ctx.cb) s_ctx.cb(APP_OTA_STATE_DONE, 100);
                APP_LOGI("ota trigger upgrade");
                cm_ota_upgrade();
            } else {
                APP_LOGE("ota write fail:%d", w);
                if (s_ctx.cb) s_ctx.cb(APP_OTA_STATE_FAILED, 0);
            }
        }
    } else {
        APP_LOGE("ota http fail");
        if (s_ctx.cb) s_ctx.cb(APP_OTA_STATE_FAILED, 0);
    }
    cm_httpclient_sync_free_data(handle);
    cm_httpclient_delete(handle);
    s_ctx.running = false;
}

int app_ota_start(const char *url, app_ota_progress_cb_t cb)
{
    if (!url || s_ctx.running) return -1;
    s_ctx.cb = cb;
    s_ctx.running = true;
    s_ctx.total_size = 0;
    s_ctx.written = 0;
    strncpy(s_ctx.url, url, sizeof(s_ctx.url) - 1);
    s_ctx.url[sizeof(s_ctx.url) - 1] = '\0';

    osThreadAttr_t tattr = {0};
    tattr.name = "ota_task";
    tattr.stack_size = 6 * 1024;
    tattr.priority = osPriorityBelowNormal;
    if (osThreadNew(ota_task, NULL, &tattr) == NULL) {
        s_ctx.running = false;
        return -2;
    }
    return 0;
}
