/**
 * @file    app_ota.c
 * @brief   APP 整包升级（需求 V1.30）：HTTP Range 分段下载 + cm_ota 分段写入 + 触发升级
 *          - 分段下载（默认 10KB/包，官方 fota 示例同款），避免大包一次性
 *            同步下载的堆大块分配失败风险（cm_http.h 同步接口注意事项）
 *          - 单包下载失败重试；连续失败擦除升级区终止（防半包固件残留）
 *          - 低电拒绝升级（需求 6.4 前置条件）；升级期间主循环冻结 LP 与
 *            定位周期（custom_main.c 门控，app_ota_is_running）
 *          - 写入完成后 cm_ota_upgrade() 校验并触发重启刷写；校验失败
 *            不重启，旧固件继续运行（回滚保障）
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "cm_os.h"
#include "cm_mem.h"
#include "cm_http.h"
#include "cm_ota.h"
#include "app_log.h"
#include "app_config.h"
#include "bsp.h"
#include "app_ota.h"

/* HTTP 分段大小：官方 fota 示例同款 10KB（examples/fota/cm_fota_demo.c
 * APP_FOTA_PER_PACKET_LEN）。同步接口响应体在共享堆上分配，单包过大
 * 有大块分配失败与碎片风险；10KB 单包峰值堆开销约 21KB（SDK 响应
 * 缓冲 + 本地拷贝缓冲），为官方验证过的稳妥值 */
#define OTA_PER_PACKET_LEN     (10 * 1024)
/* 单包下载失败重试次数（不含首次尝试）；仍失败则连续失败，擦除终止 */
#define OTA_PKT_RETRY_MAX      1
/* 单包重试间隔 */
#define OTA_PKT_RETRY_DELAY_MS 1000u

typedef struct {
    char    url[256];
    app_ota_progress_cb_t cb;
    volatile bool running;
    int     total_size;
    int     written;
} app_ota_ctx_t;

static app_ota_ctx_t s_ctx = {0};
static osEventFlagsId_t s_ota_evt = NULL;

#define OTA_EVT_RUN     0x00000001u

bool app_ota_is_running(void) { return s_ctx.running; }

/* 单次 HTTP Range 请求（每包独立 client 实例，官方 fota demo 同款生命周期）：
 * 下载 [start, end] 闭区间字节到 buf（含 SDK 响应缓冲 -> buf 的拷贝）。
 * 成功返回实际下载字节数；total_out 非空时顺带从 Content-Range 头解析
 * 升级包总大小（探测包大小与中途包大小一致性校验共用）。
 * 失败返回 -1（含：请求失败 / 状态码非 200|206 / 长度越界——服务器
 * 不支持 Range 时会回 200+整包，被 buf_size 上限拦截，视为失败） */
static int ota_fetch_range(const char *url_with_port, const char *path,
                           bool use_https, uint32_t start, uint32_t end,
                           uint8_t *buf, uint32_t buf_size, uint32_t *total_out)
{
    int ret = -1;
    cm_httpclient_handle_t handle = NULL;
    if (cm_httpclient_create((const uint8_t *)url_with_port, NULL, &handle) != CM_HTTP_RET_CODE_OK) {
        return -1;
    }
    cm_httpclient_cfg_t cfg = {0};
    cfg.ssl_enable = use_https ? 1 : 0;
    cfg.conn_timeout = 60;
    cfg.rsp_timeout = 60;
    cm_httpclient_set_cfg(handle, cfg);

    char range_hdr[48];
    snprintf(range_hdr, sizeof(range_hdr), "Range:bytes=%u-%u\r\n",
             (unsigned)start, (unsigned)end);
    (void)cm_httpclient_custom_header_set(handle, (uint8_t *)range_hdr,
                                          (uint16_t)strlen(range_hdr));

    cm_httpclient_sync_param_t param = {0};
    param.method = HTTPCLIENT_REQUEST_GET;
    param.path = (const uint8_t *)path;
    cm_httpclient_sync_response_t resp = {0};
    if (cm_httpclient_sync_request(handle, param, &resp) == CM_HTTP_RET_CODE_OK &&
        (resp.response_code == 200 || resp.response_code == 206) &&
        resp.response_content_len > 0 &&
        resp.response_content_len <= buf_size &&
        resp.response_content != NULL) {
        memcpy(buf, resp.response_content, resp.response_content_len);
        ret = (int)resp.response_content_len;
        /* 从 "Content-Range: bytes start-end/TOTAL" 解析总大小（demo 同款） */
        if (total_out != NULL && resp.response_header != NULL) {
            const char *cr = strstr((const char *)resp.response_header, "Content-Range");
            if (cr != NULL) {
                unsigned total = 0;
                if (sscanf(cr, "%*[^/]/%u", &total) == 1 && total > 0) {
                    *total_out = (uint32_t)total;
                }
            }
        }
    }

    cm_httpclient_custom_header_free(handle);
    cm_httpclient_sync_free_data(handle);
    cm_httpclient_delete(handle);
    return ret;
}

/* 一次 OTA 下载流程（常驻任务的单次执行单元）：
 * 各失败出口统一动作——擦除升级区（防半包残留）+ 上报 FAILED + 清 running；
 * 成功路径 cm_ota_upgrade() 校验通过即重启整机，不返回 */
static void ota_download_run(void)
{
    char url[256];
    strncpy(url, s_ctx.url, sizeof(url) - 1);
    url[sizeof(url) - 1] = '\0';

    /* 解析 URL 取 scheme/host/port/path */
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

    if (cm_ota_init() != 0) {
        APP_LOGE("ota init fail");
        goto fail;
    }
    if (cm_ota_firmware_erase() != 0) {
        APP_LOGE("ota erase fail");
        goto fail;
    }

    /* 探测升级包总大小（Range:bytes=0-9，读 Content-Range 头）：
     * 兼作服务器 Range 支持性探测——不支持 Range 的服务器回 200 整包，
     * 被 fetch 的 buf_size 上限拦截 */
    uint8_t *buf = cm_malloc(OTA_PER_PACKET_LEN + 1024);   /* demo 同款：多申请 1KB 余量 */
    if (buf == NULL) {
        APP_LOGE("ota malloc fail");
        goto fail;
    }
    uint32_t filelen = 0;
    int probe = ota_fetch_range(url_with_port, path, use_https, 0, 9,
                                buf, OTA_PER_PACKET_LEN + 1024, &filelen);
    if (probe < 0 || filelen == 0) {
        APP_LOGE("ota probe fail (server must support Range)");
        goto fail_free;
    }
    int32_t cap = cm_ota_get_capacity();
    if (cap > 0 && (int32_t)filelen > cap) {
        APP_LOGE("ota pkg %u > ota area capacity %d", (unsigned)filelen, (int)cap);
        goto fail_free;
    }
    cm_ota_set_otasize((int)filelen);
    s_ctx.total_size = (int)filelen;
    s_ctx.written = 0;
    APP_LOGI("ota pkg size=%u (%u pkts of %uB)",
             (unsigned)filelen,
             (unsigned)((filelen + OTA_PER_PACKET_LEN - 1) / OTA_PER_PACKET_LEN),
             (unsigned)OTA_PER_PACKET_LEN);
    if (s_ctx.cb) s_ctx.cb(APP_OTA_STATE_DOWNLOADING, 0);

    /* 分段下载 + 分段写入（cm_ota_firmware_write 为尾部累加写） */
    uint32_t offset = 0;
    while (offset < filelen) {
        uint32_t chunk = (filelen - offset > OTA_PER_PACKET_LEN)
                         ? OTA_PER_PACKET_LEN : (filelen - offset);
        int got = -1;
        for (int retry = 0; retry <= OTA_PKT_RETRY_MAX && got < 0; retry++) {
            if (retry > 0) {
                APP_LOGW("ota pkt retry at offset=%u", (unsigned)offset);
                osDelay(APP_MS_TO_TICK(OTA_PKT_RETRY_DELAY_MS));
            }
            uint32_t total_now = 0;
            got = ota_fetch_range(url_with_port, path, use_https,
                                  offset, offset + chunk - 1,
                                  buf, OTA_PER_PACKET_LEN + 1024, &total_now);
            /* 服务器包大小中途变化（升级包被替换）：数据一致性破坏，直接终止不重试 */
            if (got > 0 && total_now != 0 && total_now != filelen) {
                APP_LOGE("ota pkg size changed %u->%u, abort",
                         (unsigned)filelen, (unsigned)total_now);
                got = -1;
                break;
            }
        }
        if (got != (int)chunk) {
            APP_LOGE("ota download fail at offset=%u (want %u got %d)",
                     (unsigned)offset, (unsigned)chunk, got);
            goto fail_erase;
        }

        int w = (int)cm_ota_firmware_write((const char *)buf, (size_t)got);
        if (w != 0) {
            APP_LOGW("ota write fail:%d, retry", w);
            w = (int)cm_ota_firmware_write((const char *)buf, (size_t)got);
        }
        if (w != 0) {
            APP_LOGE("ota write fail at offset=%u", (unsigned)offset);
            goto fail_erase;
        }

        offset += (uint32_t)got;
        s_ctx.written = (int)offset;
        if (s_ctx.cb) {
            s_ctx.cb(APP_OTA_STATE_WRITING, (int)(offset * 100u / filelen));
        }
    }
    cm_free(buf);
    buf = NULL;

    /* 写入总量复核（累加写 + 分段循环双重确认，防静默丢包） */
    if (cm_ota_get_written_size() != (int32_t)filelen) {
        APP_LOGE("ota written=%d != filelen=%u",
                 (int)cm_ota_get_written_size(), (unsigned)filelen);
        goto fail;
    }

    if (s_ctx.cb) s_ctx.cb(APP_OTA_STATE_DONE, 100);
    APP_LOGI("ota pkg complete (%uB), trigger upgrade", (unsigned)filelen);
    if (cm_ota_upgrade() == 0) {
        /* 校验通过，模组即将重启刷写，不会返回到此处 */
        return;
    }
    APP_LOGE("ota upgrade trigger fail (pkg verify fail?), old fw keeps running");
    goto fail;

fail_erase:
    cm_free(buf);
    /* 连续失败：擦除升级区防半包固件残留（擦除失败再试一次，demo 同款） */
    if (cm_ota_firmware_erase() != 0) {
        (void)cm_ota_firmware_erase();
    }
    APP_LOGE("ota aborted, upgrade area erased, old fw keeps running");
    goto fail_cb;

fail_free:
    cm_free(buf);
    goto fail_cb;

fail:
    APP_LOGE("ota aborted, old fw keeps running");
fail_cb:
    if (s_ctx.cb) s_ctx.cb(APP_OTA_STATE_FAILED, 0);
    s_ctx.running = false;
}

/* 【rti 泄漏加固 2026-09-15】常驻任务：原实现每次 OTA 指令 osThreadNew、
 * 任务函数 return 退出，SDK rti 线程表项不回收（92 容量），OTA 失败 +
 * 平台反复重试场景每次泄漏 1 项（成功路径整机重启清零无碍）。
 * 现改为与 one_shot_loc_task 同款事件驱动常驻模式：任务仅创建一次
 * 终身存活，请求经 s_ctx + 事件标志传递。 */
static void ota_task(void *arg)
{
    (void)arg;
    for (;;) {
        (void)osEventFlagsWait(s_ota_evt, OTA_EVT_RUN, osFlagsWaitAny, osWaitForever);
        ota_download_run();
        /* running 已在 ota_download_run 各出口清零；
         * 若升级成功，cm_ota_upgrade() 不会返回到此处 */
    }
}

int app_ota_start(const char *url, app_ota_progress_cb_t cb)
{
    if (!url || s_ctx.running) return -1;

    /* 前置低电检查（需求 6.4 V1.30）：SOC 低于超低电阈值且非充电拒绝。
     * SOC 读不到（无电池台架/ADC 异常）不阻塞；充电中放行（外接电源
     * 无断电风险）。一次性采样，无 cm_adc_read 高频调用问题 */
    {
        int mv = 0, soc = -1;
        if (bsp_battery_read(&mv, &soc) == 0 && soc >= 0 &&
            soc < APP_SUPER_LOW_BATTERY && !bsp_charge_is_charging()) {
            APP_LOGE("ota rejected: low battery soc=%d", soc);
            return -3;
        }
    }

    if (s_ota_evt == NULL) {
        osEventFlagsAttr_t eattr = {0};
        eattr.name = "ota_evt";
        s_ota_evt = osEventFlagsNew(&eattr);
        if (s_ota_evt == NULL) {
            APP_LOGE("ota evt create fail");
            return -2;
        }
    }
    /* 常驻任务仅创建一次（running 标志防重入，不会并发重建） */
    static osThreadId_t s_ota_thread = NULL;
    if (s_ota_thread == NULL) {
        osThreadAttr_t tattr = {0};
        tattr.name = "ota_task";
        tattr.stack_size = 6 * 1024;
        tattr.priority = osPriorityBelowNormal1;   /* SDK 1.0.4 应用层优先级体系 */
        s_ota_thread = osThreadNew(ota_task, NULL, &tattr);
        if (s_ota_thread == NULL) {
            APP_LOGE("ota task create fail");
            return -2;
        }
    }

    s_ctx.cb = cb;
    s_ctx.running = true;
    s_ctx.total_size = 0;
    s_ctx.written = 0;
    strncpy(s_ctx.url, url, sizeof(s_ctx.url) - 1);
    s_ctx.url[sizeof(s_ctx.url) - 1] = '\0';

    (void)osEventFlagsSet(s_ota_evt, OTA_EVT_RUN);
    return 0;
}
