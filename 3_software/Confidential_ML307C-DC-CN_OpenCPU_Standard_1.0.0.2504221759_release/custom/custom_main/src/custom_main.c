/**
 * @file    custom_main.c
 * @brief   宠物定位器主入口
 *          集成所有模块：BSP + 模式 + 按键 + 定位 + MQTT + 离线补传 + OTA
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include "cm_sys.h"
#include "cm_os.h"
#include "cm_pm.h"
#include "cm_rtc.h"
#include "cm_modem_info.h"
#include "cm_modem.h"
#include "custom_main.h"
#include "app_config.h"
#include "app_log.h"
#include "app_utils.h"
#include "app_mode.h"
#include "app_protocol.h"
#include "app_storage.h"
#include "app_offline_cache.h"
#include "app_provisioning.h"
#include "app_mqtt_client.h"
#include "app_command.h"
#include "app_ota.h"
#include "bsp.h"

/* ===== 全局状态 ===== */
static char     g_imei[16] = "000000000000000";
static char     g_boot_id[32] = "boot_0000";
static uint32_t g_seq = 0;
static bool     g_power_on = false;
static osMutexId_t g_seq_mutex = NULL;

static app_location_t g_last_loc;
static osMutexId_t   g_loc_mutex = NULL;

static int g_last_soc = 0;
static int g_last_rssi = 0;

/* 单次定位触发标志：超省电模式下收到平台定位指令时置位，
 * 由独立任务 one_shot_loc_task 处理：打开GPS→等定位→关闭GPS→上报 */
static volatile bool g_one_shot_loc = false;
/* one-shot 任务运行中标志：防止主任务与 one_shot_loc_task 同时操作 GPS
 * 主任务 GPS 模式切换 / 关闭 GPS 前必须检查此标志 */
static volatile bool g_one_shot_running = false;
/* 当前充电状态（主循环周期更新，供 protocol 等模块使用） */
static int g_charging_status = 0;
/* GPS 是否已打开（用于超省电模式关闭/打开 GPS 节省功耗） */
static volatile bool g_gps_opened = false;
/* GPS 波特率是否已对齐：收到首条 NMEA 数据后置 true
 * 需求 2.3：GPS 芯片波特率需与串口一致，2 秒间隔反复尝试设置直到成功 */
static volatile bool g_gps_baudrate_set = false;
/* MQTT 连接成功标志：由回调设置，主循环检测后执行 subscribe/publish
 * 不能在 MQTT 回调（cmmqtt-m 任务上下文）中直接调用 MQTT API，否则重入崩溃 */
static volatile bool g_mqtt_just_connected = false;
/* 离线补传进行中：MQTT 重连后置位，主循环每轮调用一次 replay_step
 * 避免一次性阻塞主任务（缓存最大 30 条，原实现可能阻塞 6 秒） */
static bool g_offline_replay_active = false;
/* 按键长按事件暂存：key_event_cb 在 osTimer 上下文中被调用，
 * 不能直接做 osDelay/bsp_buzzer_beep 等阻塞操作，仅置位由 main_task 处理 */
static volatile bool g_key_longpress_pending = false;
/* RPC 消息暂存：回调中仅拷贝 payload + 置标志，由主循环处理（避免回调中 publish 重入） */
static volatile bool g_rpc_pending = false;
static char g_rpc_topic[64] = {0};
static char g_rpc_payload[1024] = {0};
static int  g_rpc_payload_len = 0;
/* 最后一次有效 GPS 定位更新的系统 tick（用于单次定位判断定位完成） */
static volatile uint32_t g_loc_updated_tick = 0;

/* ===== 工具：取序号 ===== */
static uint32_t next_seq(void)
{
    uint32_t s = 0;
    if (g_seq_mutex) osMutexAcquire(g_seq_mutex, osWaitForever);
    g_seq++;
    s = g_seq;
    if (g_seq_mutex) osMutexRelease(g_seq_mutex);
    app_storage_save_boot_info(g_boot_id, g_seq);
    return s;
}

/* ===== 读取信号强度 ===== */
static int read_signal_strength(void)
{
    cm_radio_info_t info = {0};
    if (cm_modem_info_radio(&info) == 0) {
        /* rssi 实际数值 = 10 *（rssi - 111）
         * rssi==0 表示模组未上报有效值，直接换算会得到 -1110 dBm（无效） */
        if (info.rssi == 0) {
            return 0;
        }
        int rssi = 10 * ((int)info.rssi - 111);
        return rssi;
    }
    return 0;
}

/* ===== 触发超省电模式下的单次定位+上报（供 app_command 调用） ===== */
void app_main_trigger_one_shot_location(void)
{
    g_one_shot_loc = true;
}

/* ===== 请求软关机（供 app_command 的 SHUTDOWN 指令调用） =====
 * 不直接调用 cm_pm_poweroff 硬关机，而是触发与长按关机相同的软关机流程：
 * 上报 OFFLINE → 关闭 GPS → 断开 MQTT → 进入 OFF 模式 → 解锁睡眠
 * 实际执行在 main_task 的 handle_key_longpress 中 */
void app_main_request_shutdown(void)
{
    g_key_longpress_pending = true;
}

/* ===== 上报一条定位 ===== */
/* 用静态缓冲区手工拼 JSON，避免 cJSON malloc/free 与 cmmqtt-m 任务并发堆操作
 * 导致堆元数据损坏 DataAbort 崩溃（newlib malloc 非线程安全） */
static void publish_location(bool is_offline_replay)
{
    app_location_t loc;
    if (g_loc_mutex) osMutexAcquire(g_loc_mutex, osWaitForever);
    loc = g_last_loc;
    if (g_loc_mutex) osMutexRelease(g_loc_mutex);

    if (loc.latitude == 0.0 && loc.longitude == 0.0) {
        APP_LOGW("LOC report without gps fix (lat=0,lon=0)");
    }

    uint32_t seq = next_seq();

    static char s_json[512];
    char msgid[APP_MSG_ID_MAX_LEN];
    char event_time[24];

    app_util_gen_loc_message_id(g_imei, g_boot_id, seq, msgid, sizeof(msgid));
    app_util_format_rfc3339(cm_rtc_get_current_time(), event_time, sizeof(event_time));

    int len = snprintf(s_json, sizeof(s_json),
        "{\"event_type\":\"location\""
        ",\"message_id\":\"%s\""
        ",\"imei\":\"%s\""
        ",\"device_sn\":\"%s\""
        ",\"event_time\":\"%s\""
        ",\"longitude\":%.6f"
        ",\"latitude\":%.6f"
        ",\"coordinate_system\":\"%s\""
        ",\"accuracy\":%d"
        ",\"satellite_count\":%d"
        ",\"source\":\"%s\""
        ",\"battery_level\":%d"
        ",\"network_type\":\"LTE\""
        ",\"signal_strength\":%d"
        ",\"speed\":%.1f"
        ",\"heading\":%.1f"
        ",\"altitude\":%.1f"
        ",\"boot_id\":\"%s\""
        ",\"sequence_no\":%lu"
        ",\"is_offline_upload\":%s}",
        msgid, g_imei, g_imei, event_time,
        loc.longitude, loc.latitude,
        loc.coord_sys[0] ? loc.coord_sys : "WGS84",
        loc.accuracy,
        loc.satellite_cnt,
        loc.source[0] ? loc.source : "GPS",
        g_last_soc, g_last_rssi,
        loc.speed, loc.heading, loc.altitude,
        g_boot_id, (unsigned long)seq,
        is_offline_replay ? "true" : "false");

    if (len <= 0 || len >= (int)sizeof(s_json)) {
        APP_LOGE("LOC #%lu json build fail/overflow", (unsigned long)seq);
        return;
    }

    if (app_mqtt_is_connected() && !is_offline_replay) {
        app_mqtt_publish_telemetry(s_json, len);
        APP_LOGI("LOC #%lu publish ok", (unsigned long)seq);
    } else {
        /* 标准版：离线缓存，待恢复连接后补传 */
        app_offline_record_t rec;
        memset(&rec, 0, sizeof(rec));
        strncpy(rec.message_id, msgid, sizeof(rec.message_id) - 1);
        strncpy(rec.event_time, event_time, sizeof(rec.event_time) - 1);
        int copy_len = len < (int)sizeof(rec.payload) ? len : (int)sizeof(rec.payload) - 1;
        memcpy(rec.payload, s_json, copy_len);
        rec.payload[copy_len] = '\0';
        rec.payload_len = copy_len;
        app_offline_cache_push(&rec);
        APP_LOGI("LOC #%lu cached (offline count=%d)",
                 (unsigned long)seq, app_offline_cache_count());
    }
}

/* ===== 上报状态 ===== */
/* 用静态缓冲区手工拼 JSON，避免 malloc/free 与 cmmqtt-m 任务的异步
 * malloc/free 并发导致堆损坏（newlib malloc 非线程安全） */
static void publish_state(const char *online_status)
{
    static char s_json[512];
    char ts[24];
    app_util_format_rfc3339(cm_rtc_get_current_time(), ts, sizeof(ts));

    int len = snprintf(s_json, sizeof(s_json),
        "{\"event_type\":\"state\""
        ",\"message_id\":\"state_%s_%s\""
        ",\"imei\":\"%s\""
        ",\"device_sn\":\"%s\""
        ",\"online_status\":\"%s\""
        ",\"event_time\":\"%s\""
        ",\"battery_level\":%d"
        ",\"firmware_version\":\"%s\""
        ",\"network_type\":\"LTE\""
        ",\"signal_strength\":%d"
        ",\"charging_status\":%d}",
        g_imei, ts, g_imei, g_imei, online_status, ts,
        g_last_soc, APP_FIRMWARE_VERSION, g_last_rssi, g_charging_status);

    if (len <= 0 || len >= (int)sizeof(s_json)) {
        APP_LOGE("state json build fail/overflow len=%d", len);
        return;
    }

    if (app_mqtt_is_connected()) {
        app_mqtt_publish_telemetry(s_json, len);
        APP_LOGI("state %s published", online_status);
    }
}

/* ===== 离线补传回调 ===== */
static void __attribute__((unused)) offline_replay_cb(const app_offline_record_t *rec)
{
    if (!rec) return;
    if (app_mqtt_is_connected()) {
        app_mqtt_publish_telemetry(rec->payload, rec->payload_len);
        APP_LOGI("replay %s", rec->message_id);
    } else {
        /* MQTT 断开：把记录 push 回缓存尾部，避免补传中数据丢失 */
        app_offline_cache_push(rec);
        APP_LOGW("replay %s re-cached (mqtt disconnected)", rec->message_id);
    }
}

/* ===== MQTT 事件回调 ===== */
static void mqtt_event_cb(app_mqtt_event_e evt, void *data)
{
    switch (evt) {
    case APP_MQTT_EVT_CONNECTED:
        /* 不在回调中调用 MQTT API（重入会导致堆损坏崩溃），
         * 仅设置标志，由 main_task 检测后执行 subscribe/publish */
        g_mqtt_just_connected = true;
        break;
    case APP_MQTT_EVT_SUBSCRIBED:
        APP_LOGI("rpc subscribed");
        break;
    case APP_MQTT_EVT_DATA_RX: {
        /* MQTT 回调（cmmqtt-m 任务上下文）中禁止调用 publish API（重入崩溃），
         * 仅拷贝 payload 到静态缓冲区并置标志，由主循环处理 */
        app_mqtt_msg_t *msg = (app_mqtt_msg_t *)data;
        if (msg && msg->payload_len > 0 &&
            msg->payload_len < (int)sizeof(g_rpc_payload) && !g_rpc_pending) {
            strncpy(g_rpc_topic, msg->topic, sizeof(g_rpc_topic) - 1);
            g_rpc_topic[sizeof(g_rpc_topic) - 1] = '\0';
            memcpy(g_rpc_payload, msg->payload, msg->payload_len);
            g_rpc_payload_len = msg->payload_len;
            g_rpc_pending = true;
        }
        break;
    }
    case APP_MQTT_EVT_DISCONNECTED:
        APP_LOGW("mqtt disconnected");
        break;
    default:
        break;
    }
}

/* ===== 等待 PS 网络注册成功（CEREG state=1/5） ===== */
static void wait_network_ready(void)
{
    cm_cereg_state_t cereg = {0};
    for (int i = 0; i < 60; i++) {  /* 最多等 60 秒 */
        if (cm_modem_get_cereg_state(&cereg) == 0) {
            if (cereg.state == 1 || cereg.state == 5) {
                APP_LOGI("network ready, cereg=%d", cereg.state);
                return;
            }
            if (i % 5 == 0) {
                APP_LOGI("waiting network, cereg=%d", cereg.state);
            }
        }
        osDelay(APP_MS_TO_TICK(1000));
    }
    APP_LOGW("network wait timeout, try mqtt anyway");
}

/* ===== Provisioning 流程 =====
 * TODO: 联调阶段直接硬编码 MQTT 凭证，跳过 SaaS provisioning。
 *       正式上线前需恢复 provisioning 流程（见下方注释块）。 */
static void provisioning_and_connect(void)
{
    app_mqtt_credential_t cred;
    memset(&cred, 0, sizeof(cred));

    /* 等待 PS 网络注册成功（CEREG state=1/5）后再连接 MQTT，
     * 避免 DNS 解析失败 / 连接错误触发 SDK 内部异常 */
    wait_network_ready();

    /* === 联调阶段：直接使用硬编码凭证 === */
    strncpy(cred.mqtt_host, "119.23.217.155", sizeof(cred.mqtt_host) - 1);
    cred.mqtt_port = 18883;
    strncpy(cred.username, "yyf588r3y8ec6evxbv6e", sizeof(cred.username) - 1);
    /* password 为空 */
    cred.password[0] = '\0';
    /* client_id 使用 IMEI（ThingsBoard ACCESS_TOKEN 模式） */
    strncpy(cred.client_id, g_imei, sizeof(cred.client_id) - 1);
    strncpy(cred.credential_type, "ACCESS_TOKEN", sizeof(cred.credential_type) - 1);
    APP_LOGI("use hardcoded credential (debug)");
    app_mqtt_connect(&cred);

    /* === 正式流程（联调通过后恢复）===
    if (app_storage_load_credential(&cred) == 0 && cred.mqtt_host[0]) {
        APP_LOGI("use saved credential");
    } else {
        APP_LOGI("no credential, provisioning...");
        app_prov_result_e r = app_provisioning_request(&cred);
        if (r != APP_PROV_OK) {
            APP_LOGE("provisioning fail:%d", r);
            return;
        }
        app_storage_save_credential(&cred);
    }
    app_mqtt_connect(&cred);
    */
}

/* ===== GPS 接收回调：解析并保存最新定位 ===== */
static void gps_rx_cb(const char *line)
{
    /* 此回调运行在 UART RX 中断上下文，禁止调用 APP_LOG（内部使用互斥锁+DEBUG 串口阻塞发送） */
    /* 收到任何 NMEA 数据说明波特率已对齐，停止 CFGPRT 重试 */
    g_gps_baudrate_set = true;
    app_location_t loc;
    memset(&loc, 0, sizeof(loc));
    if (bsp_gps_parse_nmea(line, &loc) == 1) {
        if (g_loc_mutex) osMutexAcquire(g_loc_mutex, osWaitForever);
        g_last_loc = loc;
        if (g_loc_mutex) osMutexRelease(g_loc_mutex);
        /* 记录最后一次有效 GPS 定位更新的 tick（用于单次定位判断定位完成） */
        g_loc_updated_tick = (uint32_t)osKernelGetTickCount();
    }
}

/* ===== PM 低功耗回调（不可做耗时操作） ===== */
static void pm_enter_cb(void)
{
    /* 进入低功耗前回调，勿在此处做 IO 操作 */
}

static void pm_exit_cb(uint32_t reason)
{
    /* 退出低功耗（按键唤醒）后由主任务重新锁定睡眠 */
    (void)reason;
}

/* ===== 按键回调：长按 5 秒切换开关机 =====
 * 注意：本函数在 osTimer 回调上下文中执行（key_check_timer_cb），
 * 不得调用 osDelay/bsp_buzzer_beep 等阻塞 API，否则会卡住定时器任务。
 * 实际开/关机流程由 handle_key_longpress 在 main_task 中执行。 */
static void __attribute__((unused)) key_event_cb(bool long_pressed)
{
    if (!long_pressed) return;
    g_key_longpress_pending = true;
}

/* ===== 长按按键处理：在 main_task 上下文中执行（允许阻塞） ===== */
static void handle_key_longpress(void)
{
    if (!g_power_on) {
        APP_LOGI("power on -> NORMAL");
        /* [DEBUG] PM 已禁用，排查 osa_tx_run.c 崩溃 */
        /* cm_pm_work_lock(); */
        g_power_on = true;
        bsp_buzzer_beep(2, 100, 100);
        /* RGB 绿->红->蓝 交替闪烁（需求 3：开机时） */
        bsp_rgb_set_pattern(BSP_RGB_PATTERN_PLATFORM_CMD, 3);
        app_mode_set(APP_MODE_NORMAL);
        /* 触发联网 */
        provisioning_and_connect();
    } else {
        APP_LOGI("power off");
        /* 关机前上报 OFFLINE */
        if (app_mqtt_is_connected()) {
            publish_state(APP_STATUS_OFFLINE);
            osDelay(APP_MS_TO_TICK(200));
        }
        g_power_on = false;
        bsp_buzzer_stop();
        bsp_rgb_stop_pattern();
        bsp_buzzer_beep(1, 300, 0);
        bsp_rgb_set(BSP_RGB_OFF);
        /* 关闭 GPS 节省功耗：先切超低功耗再关闭 UART */
        if (g_gps_opened) {
            bsp_gps_set_power_mode(BSP_GPS_LPMODE_ULTRA_LOW);
            osDelay(APP_MS_TO_TICK(100));
            bsp_gps_close();
            g_gps_opened = false;
        }
        app_mode_set(APP_MODE_OFF);
        app_mqtt_disconnect();
        /* [DEBUG] PM 已禁用，排查 osa_tx_run.c 崩溃 */
        /* cm_pm_work_unlock(); */
    }
}

/* ===== 超省电模式单次定位任务（独立线程，避免阻塞主任务） ===== */
#define APP_ONESHOT_TIMEOUT_MS   90000   /* 单次定位超时 90 秒 */
#define APP_ONESHOT_GPS_WARMUP_MS 3000   /* GPS 冷启动等待 */

static void one_shot_loc_task(void *arg)
{
    (void)arg;
    APP_LOGI("one-shot: open gps");
    g_one_shot_running = true;

    /* 记录打开 GPS 前的 tick，用于判断是否有新的定位数据 */
    uint32_t tick_before_open = g_loc_updated_tick;

    /* 打开 GPS UART */
    if (bsp_gps_open(gps_rx_cb) != 0) {
        APP_LOGE("one-shot: gps open fail, report last loc");
        publish_location(false);
        g_one_shot_running = false;
        return;
    }
    g_gps_opened = true;
    /* 重新打开 GPS 后波特率对齐状态失效，需要重新设置 CFGPRT（问题 12） */
    g_gps_baudrate_set = false;

    /* 切到高性能模式加速定位（ICOE CFGLPMODE,2） */
    bsp_gps_set_power_mode(BSP_GPS_LPMODE_HIGH);
    osDelay(APP_MS_TO_TICK(100));

    /* 等待 GPS 获取有效定位（带超时） */
    uint32_t start = (uint32_t)osKernelGetTickCount();
    /* 给 GPS 冷启动一些时间 */
    osDelay(APP_MS_TO_TICK(APP_ONESHOT_GPS_WARMUP_MS));
    while (1) {
        uint32_t elapsed = (uint32_t)osKernelGetTickCount() - start;
        if (elapsed >= APP_MS_TO_TICK(APP_ONESHOT_TIMEOUT_MS)) {
            APP_LOGW("one-shot: timeout %dms, report last loc", elapsed * APP_TICK_MS);
            break;
        }
        /* 判断是否收到新的有效定位 */
        if (g_loc_updated_tick != tick_before_open) {
            APP_LOGI("one-shot: location fixed after %dms", elapsed * APP_TICK_MS);
            break;
        }
        osDelay(APP_MS_TO_TICK(500));
    }

    /* 定位完成后切回超低功耗（需求 1.2.4：定位完成之后关闭定位功能） */
    bsp_gps_set_power_mode(BSP_GPS_LPMODE_ULTRA_LOW);
    osDelay(APP_MS_TO_TICK(100));
    bsp_gps_close();
    g_gps_opened = false;
    APP_LOGI("one-shot: gps closed, publishing");

    /* 上报一次定位数据 */
    publish_location(false);

    g_one_shot_running = false;
    /* 任务自行结束（CMSIS-RTOS2: 任务函数 return 即自动退出） */
    APP_LOGI("one-shot: task done");
}

/* ===== 应用层日志输出（通过 DBG 口） ===== */
void app_log_output(const char *level, const char *fmt, ...)
{
    char buf[256];
    int len = snprintf(buf, sizeof(buf), APP_LOG_TAG "[%s] ", level);
    if (len < 0 || len >= (int)sizeof(buf)) return;

    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf + len, sizeof(buf) - len, fmt, args);
    va_end(args);
    if (n < 0) return;
    len += n;
    /* 添加换行符并保证 null 终止（cm_log_printf "%s" 依赖终止符） */
    if (len + 2 < (int)sizeof(buf)) {
        buf[len++] = '\r';
        buf[len++] = '\n';
        buf[len] = '\0';
    } else {
        len = sizeof(buf) - 3;
        buf[len++] = '\r';
        buf[len++] = '\n';
        buf[len] = '\0';
    }

    cm_log_printf(0, "%s", buf);
}

/* ===== 主业务任务 ===== */
static void main_task(void *arg)
{
    (void)arg;

    /* 等待开机：在等待循环中轮询按键并处理长按事件
     * 不使用 osTimer，避免 cm_gpio_get_level 在 System Timer Thread 中触发 OSA 错误 */
    while (!g_power_on) {
        bsp_key_poll();
        if (g_key_longpress_pending) {
            g_key_longpress_pending = false;
            handle_key_longpress();
        }
        osDelay(APP_MS_TO_TICK(20));
    }

    /* 启动 GPS UART */
    if (bsp_gps_open(gps_rx_cb) != 0) {
        APP_LOGE("gps open fail");
    }
    g_gps_opened = true;
    /* 重新打开 GPS 后波特率对齐状态失效，需要重新设置 CFGPRT（问题 12） */
    g_gps_baudrate_set = false;
    /* 开机默认高性能模式加速首次定位 */
    bsp_gps_set_power_mode(BSP_GPS_LPMODE_HIGH);
    /* 需求 2.3：主动设置 GPS 芯片波特率与主控一致，2 秒间隔反复尝试直到成功 */
    bsp_gps_set_uart_baudrate(APP_GPS_UART_BAUDRATE_RATE);
    uint32_t last_cfgprt_tick = (uint32_t)osKernelGetTickCount();

    uint32_t last_loc_tick = 0;
    uint32_t last_indicator_check_tick = 0;
    uint32_t last_battery_tick = 0;      /* [FIX] 电量节流：避免每 20ms 调用 modem OSA */
    uint32_t last_rssi_tick = 0;         /* [FIX] RSSI 节流：避免每 20ms 调用 modem OSA */
    bool low_battery_announced = false;
    bool platform_cmd_active = false;  /* 平台指令触发的指示灯正在持续 */
    app_mode_e last_mode = APP_MODE_OFF;

    while (1) {
        /* 轮询按键（替代 osTimer，避免 System Timer Thread 崩溃） */
        bsp_key_poll();

        /* 处理按键长按事件（由 key_event_cb 在 bsp_key_poll 中置位）
         * 必须放在 g_power_on 检查之前，否则关机后无法再开机 */
        if (g_key_longpress_pending) {
            g_key_longpress_pending = false;
            handle_key_longpress();
            continue;
        }

        if (!g_power_on) {
            osDelay(APP_MS_TO_TICK(20));
            continue;
        }

        app_mode_e mode = app_mode_get();
        uint32_t interval = app_mode_get_loc_interval_ms();
        uint32_t now = (uint32_t)osKernelGetTickCount();

        /* MQTT 连接成功后执行 subscribe + publish ONLINE
         * 不能在 MQTT 回调中调用这些 API，故在主循环中检测标志 */
        if (g_mqtt_just_connected) {
            g_mqtt_just_connected = false;
            app_mqtt_subscribe_rpc();
            publish_state(APP_STATUS_ONLINE);
            /* 启动增量离线补传：主循环每轮调用一次 replay_step
             * 避免一次性阻塞主任务最长 6 秒 */
            g_offline_replay_active = (app_offline_cache_count() > 0);
            if (g_offline_replay_active) {
                APP_LOGI("offline replay start, count=%d", app_offline_cache_count());
            }
        }

        /* 增量离线补传：每轮主循环弹出一条上报，避免阻塞 */
        if (g_offline_replay_active) {
            if (!app_mqtt_is_connected()) {
                /* MQTT 断开：暂停补传，等下次重连后由 g_mqtt_just_connected 重新启动
                 * cb 中已将弹出记录 push 回缓存，不会丢失数据 */
                g_offline_replay_active = false;
                APP_LOGW("offline replay paused (mqtt disconnected)");
            } else {
                int r = app_offline_cache_replay_step(offline_replay_cb);
                if (r != 0) {
                    g_offline_replay_active = false;
                    APP_LOGI("offline replay done");
                }
            }
        }

        /* 处理 MQTT 回调暂存的 RPC 消息（主循环中安全调用 publish） */
        if (g_rpc_pending) {
            app_command_handle(g_rpc_topic, g_rpc_payload, g_rpc_payload_len);
            g_rpc_pending = false;
        }

        /* GPS 模式切换控制（需求 1.2.4：超省电模式不进行定位）
         * 进入超省电模式：切超低功耗 → 关闭 UART（双重省电）
         * 退出超省电模式：打开 UART → 切高性能/自适应
         * 寻狗/正常模式：高性能（快速定位）
         * 省电模式：自适应（软件自动控制功耗）
         * 注意：one-shot 任务运行中时跳过 GPS 操作，避免与 one_shot_loc_task 竞态 */
        if (mode != last_mode) {
            if (mode == APP_MODE_SUPER_SAVE && last_mode != APP_MODE_SUPER_SAVE) {
                /* 进入超省电：切超低功耗 → 关闭 UART */
                if (g_gps_opened && !g_one_shot_loc && !g_one_shot_running) {
                    bsp_gps_set_power_mode(BSP_GPS_LPMODE_ULTRA_LOW);
                    osDelay(APP_MS_TO_TICK(100));
                    bsp_gps_close();
                    g_gps_opened = false;
                    APP_LOGI("enter SUPER_SAVE, gps lpmode=0 + closed");
                }
            } else if (mode != APP_MODE_SUPER_SAVE && last_mode == APP_MODE_SUPER_SAVE) {
                /* 退出超省电：打开 UART → 根据新模式设功耗 */
                if (!g_gps_opened && !g_one_shot_running) {
                    if (bsp_gps_open(gps_rx_cb) == 0) {
                        g_gps_opened = true;
                        /* 重新打开 GPS 后波特率对齐状态失效，需重新设置 CFGPRT（问题 12） */
                        g_gps_baudrate_set = false;
                        bsp_gps_lpmode_e lpm = (mode == APP_MODE_SAVE_POWER) ?
                            BSP_GPS_LPMODE_AUTO : BSP_GPS_LPMODE_HIGH;
                        bsp_gps_set_power_mode(lpm);
                        APP_LOGI("exit SUPER_SAVE, gps opened, lpmode=%d", lpm);
                    } else {
                        APP_LOGE("exit SUPER_SAVE, gps open fail");
                    }
                }
            } else {
                /* 非超省电模式之间切换：仅调整功耗模式 */
                if (!g_one_shot_running) {
                    bsp_gps_lpmode_e lpm = (mode == APP_MODE_SAVE_POWER) ?
                        BSP_GPS_LPMODE_AUTO : BSP_GPS_LPMODE_HIGH;
                    bsp_gps_set_power_mode(lpm);
                }
            }
            last_mode = mode;
        }

        /* 寻狗模式 10 分钟自动退出 */
        if (app_mode_find_dog_expired()) {
            APP_LOGI("find-dog timeout -> NORMAL");
            app_mode_set(APP_MODE_NORMAL);
            continue;
        }

        /* 需求 2.3：GPS 芯片波特率反复尝试设置（2 秒间隔直到收到首条 NMEA）
         * 一旦收到 NMEA 数据说明波特率已对齐，停止重试 */
        if (g_gps_opened && !g_gps_baudrate_set) {
            uint32_t now_baud = (uint32_t)osKernelGetTickCount();
            if ((now_baud - last_cfgprt_tick) >= APP_MS_TO_TICK(2000)) {
                last_cfgprt_tick = now_baud;
                bsp_gps_set_uart_baudrate(APP_GPS_UART_BAUDRATE_RATE);
                APP_LOGI("gps baudrate retry CFGPRT %u", APP_GPS_UART_BAUDRATE_RATE);
            }
        }

        /* 标准版：电量周期采样 + 超低电量强制切超级省电
         * [FIX] 每 5000ms 采样一次：cm_adc_read / cm_battery_get_soc 经由 modem OSA tx 路径，
         * 主循环 20ms 一次无节流调用会导致 OSA tx buffer 重入，
         * Tx Status 被置 0x4 引发 System Timer Thread osa_tx_run.c:241 崩溃 */
        int mv = 0, soc = -1;
        if ((now - last_battery_tick) >= APP_MS_TO_TICK(5000)) {
            last_battery_tick = now;
            if (bsp_battery_read(&mv, &soc) == 0) {
                if (soc != g_last_soc) {
                    APP_LOGI("battery mv=%d soc=%d", mv, soc);
                    g_last_soc = soc;
                }
                if (soc >= 0 && soc < APP_SUPER_LOW_BATTERY) {
                    if (mode != APP_MODE_SUPER_SAVE) {
                        APP_LOGW("super low battery, force SUPER_SAVE");
                        app_mode_set(APP_MODE_SUPER_SAVE);
                        /* 不 continue，后续可能还有低电指示灯等处理 */
                    }
                }
            }
        }

        /* 标准版：低电指示灯周期维护（每 2 秒）
         * 充电检测功能暂未实现，仅基于 SOC 维护低电慢闪 */
        if ((now - last_indicator_check_tick) >= APP_MS_TO_TICK(2000)) {
            last_indicator_check_tick = now;
            g_charging_status = 0;  /* 充电功能未实现，固定为放电 */
            if (!platform_cmd_active) {
                if (g_last_soc >= 0 && g_last_soc < APP_LOW_BATTERY_THRESHOLD) {
                    if (!low_battery_announced) {
                        bsp_rgb_set_pattern(BSP_RGB_PATTERN_LOW_BATTERY, 0);
                        low_battery_announced = true;
                    }
                } else if (low_battery_announced) {
                    /* 电量恢复正常，关闭低电指示 */
                    bsp_rgb_stop_pattern();
                    low_battery_announced = false;
                }
            }
        }

        /* 信号强度周期采样
         * [FIX] 每 5000ms 采样一次：cm_modem_info_radio 经由 modem OSA tx 路径，
         * 主循环 20ms 一次无节流调用会导致 OSA tx buffer 重入，
         * Tx Status 被置 0x4 引发 System Timer Thread osa_tx_run.c:241 崩溃 */
        if ((now - last_rssi_tick) >= APP_MS_TO_TICK(5000)) {
            last_rssi_tick = now;
            g_last_rssi = read_signal_strength();
        }

        /* 周期定位（超省电模式不主动定位） */
        if (interval > 0 && (now - last_loc_tick) >= APP_MS_TO_TICK(interval)) {
            last_loc_tick = now;
            publish_location(false);
        }

        /* 超省电模式下单次定位触发（收到平台定位指令后）
         * 需求 1.2.4：打开 GPS → 等定位完成 → 关闭 GPS → 上报 */
        if (g_one_shot_loc) {
            g_one_shot_loc = false;
            APP_LOGI("one-shot location triggered (super save)");
            /* 启动独立任务，避免阻塞主循环 */
            osThreadAttr_t attr = {0};
            attr.name = "one_shot";
            attr.stack_size = 4096;
            attr.priority = osPriorityBelowNormal;
            osThreadId_t tid = osThreadNew(one_shot_loc_task, NULL, &attr);
            if (tid == NULL) {
                APP_LOGE("one-shot task create fail, fallback direct publish");
                publish_location(false);
            }
            last_loc_tick = now;
        }

        /* 周期心跳日志，确认程序正常运行 */
        static uint32_t last_hb_tick = 0;
        if (now - last_hb_tick >= APP_MS_TO_TICK(10000)) {
            last_hb_tick = now;
            /* 读取当前 GPS 定位状态用于心跳日志 */
            app_location_t hb_loc;
            if (g_loc_mutex) osMutexAcquire(g_loc_mutex, osWaitForever);
            hb_loc = g_last_loc;
            if (g_loc_mutex) osMutexRelease(g_loc_mutex);
            APP_LOGI("alive mode=%d rssi=%d gps=%s sat=%d lat=%.5f lon=%.5f",
                     (int)mode, g_last_rssi,
                     (hb_loc.latitude != 0.0 || hb_loc.longitude != 0.0) ? "FIX" : "NOFIX",
                     hb_loc.satellite_cnt, hb_loc.latitude, hb_loc.longitude);
        }

        osDelay(APP_MS_TO_TICK(20));  /* 20ms 间隔支持按键轮询 */
    }
}

/* ===== 开机初始化 ===== */
static void system_init(void)
{
    osMutexAttr_t mattr = {0};
    mattr.name = "seq_mtx";
    g_seq_mutex = osMutexNew(&mattr);

    osMutexAttr_t lattr = {0};
    lattr.name = "loc_mtx";
    g_loc_mutex = osMutexNew(&lattr);

    app_mode_init();
    app_offline_cache_init();
    app_mqtt_init(mqtt_event_cb);

    /* [DEBUG] 暂时禁用 PM 以排查 osa_tx_run.c 崩溃：
     * SDK 示例不使用 cm_pm_init/cm_pm_work_lock，PM 内部定时器
     * 可能在 System Timer Thread 中触发崩溃。若禁用后崩溃消失，
     * 则确认 PM 为根因，需寻找替代方案管理睡眠 */
    /* cm_pm_cfg_t pm_cfg = { pm_enter_cb, pm_exit_cb }; */
    /* cm_pm_init(pm_cfg); */
    /* cm_pm_work_lock(); */
    (void)pm_enter_cb;
    (void)pm_exit_cb;

    if (cm_sys_get_imei(g_imei) != 0) {
        strcpy(g_imei, "000000000000000");
    }
    app_command_set_imei(g_imei);
    APP_LOGI("IMEI=%s ver=%s", g_imei, APP_FIRMWARE_VERSION);

    /* 每次启动生成新 boot_id 并清零序号（旧 boot_id 无需保留） */
    app_util_gen_boot_id(g_boot_id, sizeof(g_boot_id));
    g_seq = 0;
    app_storage_save_boot_info(g_boot_id, g_seq);
    APP_LOGI("boot_id=%s", g_boot_id);

    bsp_init();
    /* 标准版：注册按键回调（长按5秒开关机） */
    bsp_key_register_cb(key_event_cb);
}

/* ====================================================================
 * OpenCPU 入口
 * ==================================================================== */
int cm_opencpu_entry(void *param)
{
    (void)param;

    system_init();

    /* 需求 1.1：装上电池、复位后默认为关机模式，等待长按按键 5 秒开机 */
    /* g_power_on 保持 false，APP_MODE_OFF 为默认值，main_task 会阻塞等待开机 */
    APP_LOGI("pet tracker start in OFF mode, waiting for power-on key");

    osThreadAttr_t task_attr = {0};
    task_attr.name = "pet_main";
    task_attr.stack_size = 12 * 1024;
    task_attr.priority = osPriorityNormal;
    if (osThreadNew(main_task, NULL, &task_attr) == NULL) {
        APP_LOGE("main task create fail");
        return -1;
    }

    return 0;
}
