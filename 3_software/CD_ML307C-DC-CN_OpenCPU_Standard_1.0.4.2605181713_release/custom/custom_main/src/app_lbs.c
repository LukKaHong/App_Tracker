/**
 * @file    app_lbs.c
 * @brief   LBS & WiFi 原始参数采集上报实现（需求 2.2）
 * @details 流程（参考 SDK examples/wifiscan 官方 demo）：
 *          1. cm_modem_get_cell_info 采集服务小区 + 邻区原始参数（需在线）
 *          2. WiFi 扫描：CFUN=5 关闭 4G 射频（模组掉网，扫描更稳定）
 *             → cm_wifiscan_start 异步扫描 → CFUN=1 恢复 → 等待 PDP 激活
 *          3. 按高德智能硬件定位字段格式上报平台（平台调高德解算坐标）：
 *             bts="mcc,mnc,lac,cellid,signal"（服务小区）
 *             nearbts="...|..."（邻区，| 分隔）
 *             macs="AA:BB:CC:DD:EE:FF,rssi|..."（WiFi AP，| 分隔）
 *
 *          当前 APP_LBS_WIFI_ENABLE=0 暂时屏蔽 WiFi 扫描：
 *          仅采集基站参数上报（不掉网、MQTT 保持连接），报文不含 macs 字段。
 *          恢复 WiFi 采集改 app_config.h 中该开关为 1 即可。
 *
 *          WiFi 扫描受限频控制（默认 5 分钟最小间隔）以降低掉网与功耗影响；
 *          离线时 LBS 原始参数不上缓存（无线环境随时间变化，过期原始参数
 *          解算出的坐标无意义，且报文大于离线缓存单条容量）。
 */
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "cm_os.h"
#include "cm_rtc.h"
#include "cm_modem.h"
#include "cm_modem_info.h"
#include "cm_wifiscan.h"
#include "app_log.h"
#include "app_config.h"
#include "app_utils.h"
#include "app_protocol.h"
#include "app_mqtt_client.h"
#include "app_lbs.h"

/* 由 custom_main.c 提供的通用上报接口（在线直发 / 离线缓存） */
extern int app_main_publish_telemetry(const char *json, int len,
                                      const char *msgid, const char *event_time);

#define LBS_EVT_SCAN_DONE   (1u << 0)

static char     g_imei[16] = "000000000000000";
static char     g_boot_id[32] = "boot_0000";
static volatile bool s_running = false;          /* 采集任务运行中 */
static uint32_t s_lbs_seq = 0;                   /* LBS 上报序号（message_id） */

#if APP_LBS_WIFI_ENABLE
static osEventFlagsId_t s_scan_evt = NULL;
static uint32_t s_last_wifi_scan_tick = 0;       /* 上次 WiFi 扫描时刻（限频） */

/* WiFi 扫描结果缓存（回调上下文中拷贝） */
static cm_wifi_scan_info_t s_wifi_result;
static volatile bool s_scan_done = false;
#endif

void app_lbs_set_imei(const char *imei)
{
    if (imei) {
        strncpy(g_imei, imei, sizeof(g_imei) - 1);
        g_imei[sizeof(g_imei) - 1] = '\0';
    }
}

void app_lbs_set_boot_id(const char *boot_id)
{
    if (boot_id) {
        strncpy(g_boot_id, boot_id, sizeof(g_boot_id) - 1);
        g_boot_id[sizeof(g_boot_id) - 1] = '\0';
    }
}

int app_lbs_init(void)
{
#if APP_LBS_WIFI_ENABLE
    if (!s_scan_evt) {
        s_scan_evt = osEventFlagsNew(NULL);
    }
    s_last_wifi_scan_tick = 0;
#endif
    s_running = false;
    s_lbs_seq = 0;
#if APP_LBS_WIFI_ENABLE
    return s_scan_evt ? 0 : -1;
#else
    return 0;
#endif
}

bool app_lbs_is_running(void)
{
    return s_running;
}

#if APP_LBS_WIFI_ENABLE
/* ===== WiFi 扫描回调（SDK 任务上下文）：拷贝结果并置完成标志 ===== */
static void wifi_scan_cb(cm_wifi_scan_info_t *param, void *user_param)
{
    (void)user_param;
    if (param) {
        memcpy(&s_wifi_result, param, sizeof(s_wifi_result));
    }
    s_scan_done = true;
    if (s_scan_evt) osEventFlagsSet(s_scan_evt, LBS_EVT_SCAN_DONE);
}
#endif /* APP_LBS_WIFI_ENABLE */

/* ===== 采集基站原始参数（需网络在线，须在 CFUN=5 之前调用）=====
 * bts:     "mcc,mnc,lac,cellid,signal"（服务小区，signal 为负数 dBm）
 * nearbts: 邻区列表，组内格式同 bts，组间以 | 分隔
 * 返回 0 成功，<0 失败 */
static int collect_cell_info(char *bts, size_t bts_size,
                             char *nearbts, size_t near_size)
{
    cm_cell_info_t cells[8];
    memset(cells, 0, sizeof(cells));

    int n = cm_modem_get_cell_info(cells, 8);
    if (n <= 0) {
        APP_LOGW("lbs: get cell info fail:%d", n);
        return -1;
    }
    if (n > 8) n = 8;

    /* 找服务小区（primary_cell），未标记时取第一个 */
    int primary_idx = 0;
    for (int i = 0; i < n; i++) {
        if (cells[i].primary_cell) {
            primary_idx = i;
            break;
        }
    }

    /* rsrp 原始值转实际 dBm：SDK 公式 10*(rsrp-140) 输出单位为 0.1dBm，
     * 高德协议要求 signal 单位 dBm，故实际 dBm = rsrp - 140
     * （实测 2026-08-20：原始 60 → -80dBm，与 RSSI -64dBm 相符） */
    cm_cell_info_t *p = &cells[primary_idx];
    int signal_dbm = (int)p->rsrp - 140;
    snprintf(bts, bts_size, "%s,%s,%u,%lu,%d",
             (const char *)p->mcc, (const char *)p->mnc,
             p->tac, (unsigned long)p->cid, signal_dbm);

    /* 邻区列表 */
    nearbts[0] = '\0';
    for (int i = 0; i < n; i++) {
        if (i == primary_idx) continue;
        char one[64];
        int sig = (int)cells[i].rsrp - 140;
        snprintf(one, sizeof(one), "%s,%s,%u,%lu,%d",
                 (const char *)cells[i].mcc, (const char *)cells[i].mnc,
                 cells[i].tac, (unsigned long)cells[i].cid, sig);
        if (nearbts[0] != '\0') {
            strncat(nearbts, "|", near_size - strlen(nearbts) - 1);
        }
        strncat(nearbts, one, near_size - strlen(nearbts) - 1);
    }
    return 0;
}

#if APP_LBS_WIFI_ENABLE
/* ===== WiFi 扫描：CFUN=5 掉网扫描 → CFUN=1 恢复 → 等 PDP 激活 =====
 * 返回扫描到的 AP 数量（0 = 无有效结果） */
static int do_wifi_scan(void)
{
    uint8_t round = APP_LBS_WIFI_SCAN_ROUND;
    uint8_t max_count = APP_LBS_WIFI_SCAN_MAX_COUNT;
    uint8_t timeout_s = APP_LBS_WIFI_SCAN_TIMEOUT_S;
    uint8_t priority = CM_WIFI_SCAN_WIFI_HIGH;  /* WiFi 优先（SDK demo 建议） */

    if (cm_wifiscan_cfg(CM_WIFI_SCAN_CFG_ROUND, &round) != 0 ||
        cm_wifiscan_cfg(CM_WIFI_SCAN_CFG_MAX_COUNT, &max_count) != 0 ||
        cm_wifiscan_cfg(CM_WIFI_SCAN_CFG_TIMEOUT, &timeout_s) != 0 ||
        cm_wifiscan_cfg(CM_WIFI_SCAN_CFG_PRIORITY, &priority) != 0) {
        APP_LOGW("lbs: wifiscan cfg fail");
        return -1;
    }

    /* WiFi 扫描期间关闭 4G 射频（模组掉网），扫描更稳定 */
    if (cm_modem_set_cfun(5) < 0) {
        APP_LOGW("lbs: set cfun 5 fail");
        return -1;
    }

    memset(&s_wifi_result, 0, sizeof(s_wifi_result));
    s_scan_done = false;
    /* SDK cm_os.h 未开放 osEventFlagsClear，用 0 超时等待消费掉残留标志 */
    if (s_scan_evt) {
        osEventFlagsWait(s_scan_evt, LBS_EVT_SCAN_DONE, osFlagsWaitAny, 0);
    }

    if (cm_wifiscan_start(wifi_scan_cb, NULL) != 0) {
        APP_LOGW("lbs: wifiscan start fail");
        cm_modem_set_cfun(1);
        return -1;
    }

    /* 等待扫描完成回调（总超时 + 5 秒缓冲） */
    if (s_scan_evt) {
        osEventFlagsWait(s_scan_evt, LBS_EVT_SCAN_DONE, osFlagsWaitAny,
                         APP_MS_TO_TICK((APP_LBS_WIFI_SCAN_TIMEOUT_S + 5) * 1000));
    }
    /* 超时未完成则中断扫描 */
    cm_wifiscan_stop();

    /* 恢复网络 */
    if (cm_modem_set_cfun(1) < 0) {
        osDelay(APP_MS_TO_TICK(1000));
        cm_modem_set_cfun(1);
    }

    /* 等待 PDP 激活 */
    int pdp_wait_cnt = APP_LBS_PDP_WAIT_TIMEOUT_S * 5;  /* 200ms 一次 */
    while (pdp_wait_cnt-- > 0) {
        if (cm_modem_get_pdp_state(1) == 1) break;
        osDelay(APP_MS_TO_TICK(200));
    }
    /* PDP 激活到可数据通信需适当等待（SDK demo 建议） */
    osDelay(APP_MS_TO_TICK(1000));

    if (!s_scan_done) {
        APP_LOGW("lbs: wifiscan timeout");
        return 0;
    }
    APP_LOGI("lbs: wifiscan got %d ap", s_wifi_result.bssid_number);
    return s_wifi_result.bssid_number;
}

/* ===== 组装 macs 字符串（高德格式：MAC,rssi 以 | 分隔）===== */
static void build_macs_string(char *macs, size_t size)
{
    macs[0] = '\0';
    int n = s_wifi_result.bssid_number;
    if (n > MAX_BSSID_NUM_TOTAL) n = MAX_BSSID_NUM_TOTAL;

    for (int i = 0; i < n; i++) {
        cm_wifi_scan_cell_info_t *ap = &s_wifi_result.channel_cell_list[i];
        uint8_t *b = ap->bssid;
        char one[32];
        snprintf(one, sizeof(one), "%02X:%02X:%02X:%02X:%02X:%02X,%d",
                 b[0], b[1], b[2], b[3], b[4], b[5], (int)ap->rssi);
        if (macs[0] != '\0') {
            strncat(macs, "|", size - strlen(macs) - 1);
        }
        strncat(macs, one, size - strlen(macs) - 1);
    }
}
#endif /* APP_LBS_WIFI_ENABLE */

/* ===== 最近一次采集的基站参数缓存（按定位周期持续刷新）===== */
static char s_cached_bts[64] = "";
static char s_cached_nearbts[512] = "";
static bool s_cached_valid = false;

/* 查询缓存的服务小区 bts 字符串（"mcc,mnc,lac,cellid,signal"）
 * 无缓存返回 NULL。仅用于日志/调试显示 */
const char *app_lbs_get_cached_bts(void)
{
    return s_cached_valid ? s_cached_bts : NULL;
}

/* ===== 采集 + 上报一次 LBS&WiFi 原始参数 ===== */
static void lbs_report(bool force_wifi, bool report)
{
    static char bts[64];
    static char nearbts[512];
#if APP_LBS_WIFI_ENABLE
    static char macs[APP_LBS_URL_BUF_SIZE];
#endif
    static char json[APP_LBS_JSON_BUF_SIZE];
    char msgid[APP_MSG_ID_MAX_LEN];
    char event_time[24];

    /* 1. 基站原始参数（需在线，必须在 CFUN=5 掉网前采集） */
    bool has_bts = (collect_cell_info(bts, sizeof(bts),
                                      nearbts, sizeof(nearbts)) == 0);

    /* 2. WiFi 扫描（限频：默认最小间隔 5 分钟，force_wifi 可跳过限制） */
    bool has_macs = false;
#if APP_LBS_WIFI_ENABLE
    uint32_t now = (uint32_t)osKernelGetTickCount();
    bool wifi_allowed = force_wifi || (s_last_wifi_scan_tick == 0) ||
        ((now - s_last_wifi_scan_tick) >= APP_MS_TO_TICK(APP_LBS_WIFI_SCAN_MIN_INTERVAL_MS));
    if (wifi_allowed) {
        s_last_wifi_scan_tick = now;
        int ap_cnt = do_wifi_scan();
        if (ap_cnt > 0) {
            build_macs_string(macs, sizeof(macs));
            has_macs = true;
        }
    } else {
        APP_LOGD("lbs: wifi scan skipped (interval limit)");
    }
#else
    (void)force_wifi;   /* WiFi 屏蔽期间参数不使用 */
    APP_LOGD("lbs: wifi scan disabled (APP_LBS_WIFI_ENABLE=0)");
#endif

    if (!has_bts && !has_macs) {
        APP_LOGW("lbs: no cell & no wifi data, skip report");
        return;
    }

    /* 缓存本周期采集结果（GPS 有效性判断在主循环，本模块不感知） */
    if (has_bts) {
        strncpy(s_cached_bts, bts, sizeof(s_cached_bts) - 1);
        s_cached_bts[sizeof(s_cached_bts) - 1] = '\0';
        strncpy(s_cached_nearbts, nearbts, sizeof(s_cached_nearbts) - 1);
        s_cached_nearbts[sizeof(s_cached_nearbts) - 1] = '\0';
        s_cached_valid = true;
    }

    if (!report) {
        /* 仅采集模式：刷新缓存后直接返回，不上报 */
        APP_LOGD("lbs: collect only, cached bts (report suppressed)");
        return;
    }

    /* 3. 组装上报 JSON（协议 5.1 location 事件 + 需求 2.2 原始参数扩展字段）
     * LBS 模式设备无坐标，longitude/latitude 填 0，
     * 由平台根据 bts/nearbts/macs 调高德解算坐标 */
    s_lbs_seq++;
    snprintf(msgid, sizeof(msgid), "lbs_%s_%s_%lu",
             g_imei, g_boot_id, (unsigned long)s_lbs_seq);
    app_util_format_rfc3339(cm_rtc_get_current_time(), event_time, sizeof(event_time));

    const char *source = (has_bts && has_macs) ? "MIXED" : (has_macs ? "WIFI" : "LBS");
    int len = snprintf(json, sizeof(json),
        "{\"event_type\":\"location\""
        ",\"message_id\":\"%s\""
        ",\"imei\":\"%s\""
        ",\"device_sn\":\"%s\""
        ",\"event_time\":\"%s\""
        ",\"longitude\":0"
        ",\"latitude\":0"
        ",\"source\":\"%s\""
        ",\"bts\":\"%s\""
        ",\"nearbts\":\"%s\""
#if APP_LBS_WIFI_ENABLE
        ",\"macs\":\"%s\""
#endif
        ",\"boot_id\":\"%s\""
        ",\"sequence_no\":%lu"
        ",\"is_offline_upload\":false}",
        msgid, g_imei, g_imei, event_time, source,
        has_bts ? bts : "",
        (has_bts && nearbts[0]) ? nearbts : "",
#if APP_LBS_WIFI_ENABLE
        has_macs ? macs : "",
#endif
        g_boot_id, (unsigned long)s_lbs_seq);

    if (len <= 0 || len >= (int)sizeof(json)) {
        APP_LOGE("lbs: json build fail/overflow len=%d", len);
        return;
    }

    /* 4. 上报：在线直发；离线丢弃（LBS 原始参数时效性强，
     * 过期参数解算坐标无意义，且报文大于离线缓存单条容量） */
    if (!app_mqtt_is_connected()) {
        APP_LOGW("lbs: offline, drop raw params report");
        return;
    }
    app_main_publish_telemetry(json, len, msgid, event_time);
    APP_LOGI("lbs: report seq=%lu bts=%d macs=%d",
             (unsigned long)s_lbs_seq, (int)has_bts, (int)has_macs);
}

/* ===== 采集上报任务（独立线程，WiFi 扫描阻塞约 30~60 秒）===== */
static void lbs_task(void *arg)
{
    /* arg 位编码：bit0 = force_wifi，bit1 = report */
    bool force_wifi = (((uintptr_t)arg) & 0x1u) != 0;
    bool report = (((uintptr_t)arg) & 0x2u) != 0;
    APP_LOGI("lbs: task start (force_wifi=%d report=%d)", (int)force_wifi, (int)report);
    lbs_report(force_wifi, report);
    s_running = false;
    APP_LOGI("lbs: task done");
}

int app_lbs_trigger(bool force_wifi, bool report)
{
    if (s_running) {
        APP_LOGW("lbs: task busy, skip");
        return -1;
    }
    s_running = true;

    uintptr_t arg = (force_wifi ? 0x1u : 0x0u) | (report ? 0x2u : 0x0u);
    osThreadAttr_t attr = {0};
    attr.name = "lbs_task";
    attr.stack_size = 8 * 1024;
    attr.priority = osPriorityBelowNormal1;   /* SDK 1.0.4 应用层优先级体系 */
    if (osThreadNew(lbs_task, (void *)arg, &attr) == NULL) {
        APP_LOGE("lbs: task create fail");
        s_running = false;
        return -2;
    }
    return 0;
}
