/**
 * @file    custom_main.c
 * @brief   宠物定位器主入口（需求文档 V1.12）
 *          集成：BSP + 模式 + 定位 + LBS/WiFi + MQTT + 离线补传 + OTA + 计步 + LP
 *
 *          与旧版本差异（需求 V1.8~V1.12）：
 *          - 删除软件关机模式：电源键接 PWR_ON/OFF 硬件开关机，软件不干预；
 *            上电即工作，进入 flash 恢复的模式（首次默认看护）
 *          - 完整 LP 流程（仅看护/省电模式）：RTC 闹钟按定位周期唤醒 →
 *            GNSS 上电定位 → 上报 → GNSS 断电进 LP；引脚睡眠态由 bsp 层
 *            pad 级配置（SLEEP_FLOAT）自动生效
 *          - LBS 参数每定位周期上报（无论 GNSS 是否有效，需求 V1.2）
 *          - GNSS 有效性：上次有效定位距今 > 2 倍定位周期 → GPS 字段无效
 *          - WiFi 扫描触发：GNSS 连续 2 周期无效（看护/省电），限频 5 分钟，
 *            静止期间复用缓存 macs 不扫描（需求 V1.12）
 *          - 充电检测：CHRG_State Pin87 轮询 + LP 边沿唤醒，变化即时上报；
 *            休眠模式充电自动恢复看护
 *          - 计步：QMA6100P 每定位周期读取随定位上报；本地 0 点清零
 *            （依赖 NTP 对时）；静止判定（增量<=3 步连续 2 周期）跳 GNSS
 *          - 复位原因 / 设备信息（app_ver/hw_ver/modem_ver/iccid）随
 *            ONLINE 状态事件上报（需求 9 / 6.7）
 *
 *          时间基准说明：LP 睡眠期间 OS tick 冻结，定位/心跳到期判定使用
 *          RTC UTC 秒（cm_rtc_get_current_time，LP 中持续走时）；
 *          唤醒态短超时（GNSS 90s 等）使用 OS tick。
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <time.h>
#include "cm_sys.h"
#include "cm_os.h"
#include "cm_pm.h"
#include "cm_gpio.h"
#include "cm_iomux.h"
#include "cm_rtc.h"
#include "cm_sim.h"
#include "cm_modem_info.h"
#include "cm_modem.h"
#include "cm_ntp.h"
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
#include "app_lbs.h"
#include "bsp.h"

/* ===== 唤醒事件标志（LP 睡眠等待）===== */
#define WAKE_EVT_RTC        (1u << 0)   /* RTC 闹钟（定位/心跳到期） */
#define WAKE_EVT_NET        (1u << 1)   /* 网络下行（平台指令寻呼唤醒） */
#define WAKE_EVT_PM         (1u << 2)   /* PM 退出回调（含 CHRG 边沿等 GPIO 唤醒） */
#define WAKE_EVT_ALL        (WAKE_EVT_RTC | WAKE_EVT_NET | WAKE_EVT_PM)

/* ===== LP 状态机（看护/省电/休眠模式）=====
 * 关键设计：唤醒后必须先回主循环处理唤醒事务（RPC 指令/充电检测等），
 * 再由状态机决策是否重新入睡（需求 step6：平台指令寻呼唤醒，处理完指令
 * 后重新进入 LP）。因此 SLEEP（阻塞睡眠）与 WOKE（唤醒事务处理）分离，
 * 睡眠只发生在主循环确认无待办事务之后。 */
typedef enum {
    LP_ST_IDLE = 0,     /* 非 LP 状态（寻宠/遛宠，GNSS 常开） */
    LP_ST_WOKE,         /* LP 唤醒态：处理唤醒事务（心跳/定位到期），随后决策 */
    LP_ST_SLEEP,        /* 进入阻塞睡眠：GNSS 断电，等待唤醒事件 */
    LP_ST_GNSS_WAIT,    /* GNSS 已上电，等待定位（最长 APP_GNSS_FIX_TIMEOUT_MS） */
} lp_state_e;

/* ===== 全局状态 ===== */
static char     g_imei[CM_IMEI_LEN] = "000000000000000";
static char     g_boot_id[32] = "boot_0000";
static uint32_t g_seq = 0;
static osMutexId_t g_seq_mutex = NULL;

static app_location_t g_last_loc;
static osMutexId_t   g_loc_mutex = NULL;

static int g_last_soc = -1;   /* -1 = 未知 */
static int g_last_rssi = 0;
static int g_charging_status = 0;

/* 单次定位触发标志：休眠模式下收到平台状态读取/定位指令时置位，
 * 由独立任务 one_shot_loc_task 处理：打开GPS→等定位→关闭GPS→上报 */
static volatile bool g_one_shot_loc = false;
/* one-shot 任务运行中标志：防止主任务与 one_shot_loc_task 同时操作 GPS */
static volatile bool g_one_shot_running = false;
/* GPS 是否已打开 */
static volatile bool g_gps_opened = false;
/* GPS 波特率是否已对齐：收到首条 NMEA 数据后置 true
 * 需求 2.1：GPS 芯片波特率需与串口一致，2 秒间隔反复尝试设置直到成功 */
static volatile bool g_gps_baudrate_set = false;
/* GPS 当前实际应用的功耗模式（CFGLPMODE 值，-1=未应用/GPS 已关闭） */
static int g_gps_lpmode_applied = -1;

/* MQTT 连接成功标志：由回调设置，主循环检测后执行 subscribe/publish
 * 不能在 MQTT 回调（cmmqtt-m 任务上下文）中直接调用 MQTT API，否则重入崩溃 */
static volatile bool g_mqtt_just_connected = false;
/* 离线补传进行中 */
static bool g_offline_replay_active = false;
/* RPC 消息暂存：回调中仅拷贝 payload + 置标志，由主循环处理（避免回调中 publish 重入） */
static volatile bool g_rpc_pending = false;
static char g_rpc_topic[64] = {0};
static char g_rpc_payload[1024] = {0};
static int  g_rpc_payload_len = 0;
/* 最后一次有效 GPS 定位更新的系统 tick（唤醒态有效性判断）与 UTC 秒（跨 LP 判断） */
static volatile uint32_t g_loc_updated_tick = 0;
static uint64_t g_loc_updated_utc = 0;

/* ===== LP 调度状态 ===== */
static osEventFlagsId_t g_wake_evt = NULL;
static lp_state_e s_lp_state = LP_ST_IDLE;
static bool s_lp_slept_once = false;         /* 已经历过至少一次 LP 睡眠（区分
                                              * 开机首轮 WOKE 与真实唤醒） */
static uint64_t s_next_loc_due_utc = 0;    /* 下次定位到期（UTC 秒） */
static uint64_t s_next_hb_due_utc  = 0;    /* 下次心跳到期（UTC 秒，0=不需要） */
static uint32_t s_gnss_wait_start_tick = 0;/* LP_ST_GNSS_WAIT 进入时刻 */

/* ===== 计步与静止判定（需求 8）===== */
static uint32_t g_steps = 0;               /* 当日累计步数（0 点清零后由 STEP_CNT 直接读出） */
static uint32_t g_last_step_cnt = 0;       /* 上一定位周期 STEP_CNT 原始值 */
static int      g_still_cycles = 0;        /* 连续静止周期计数 */
static bool     g_still_active = false;    /* 静止状态（连续 APP_STILL_CYCLES 周期静止） */
static uint32_t g_last_cellid = 0;         /* 上周期服务小区 cellid（LBS 安全网） */
static bool     g_last_cellid_valid = false;
static int      g_last_clear_day = -1;     /* 上次清零的本地日序号（UTC+8） */

/* ===== WiFi 触发（需求 V1.12：GNSS 连续 N 周期无效）===== */
static int g_gnss_invalid_cycles = 0;

/* 前向声明：充电检测（定义在本文件后部，LP 唤醒处理中强制检查） */
static void charge_poll(void);
/* 前向声明：GPS 电源管理（定义在本文件后部，关机流程中调用） */
static void gps_power_close(void);
/* 前向声明：GPS 接收回调（定义在本文件后部，gps_power_open 注册用） */
static void gps_rx_cb(const char *line);

/* ===== NTP 对时（需求 6.5）===== */
static bool     g_ntp_synced = false;      /* 对时成功前计步不做 0 点清零 */
static uint64_t g_last_ntp_utc = 0;

/* ===== 复位原因与设备信息（需求 9 / 6.7，首次 ONLINE 上报）===== */
static int  g_reset_reason = CM_PM_UNKNOWN;
static bool g_device_info_reported = false;
static char g_modem_ver[CM_VER_LEN] = {0};
static char g_iccid[24] = {0};

/* ===== 工具：取序号 =====
 * 序号不落 flash：boot_id 每次开机重新生成（时间戳+随机数），
 * message_id 唯一性由 boot_id 保证，持久化 seq 纯写不读（开机即清零）。
 * 历史教训 2026-09-03：每条报文写 flash 不仅磨损，且复位撞上写入窗口
 * 曾致 PTN0 littlefs 根目录损坏（Corrupted dir pair），工作模式丢失 */
static uint32_t next_seq(void)
{
    uint32_t s = 0;
    if (g_seq_mutex) osMutexAcquire(g_seq_mutex, osWaitForever);
    g_seq++;
    s = g_seq;
    if (g_seq_mutex) osMutexRelease(g_seq_mutex);
    return s;
}

/* ===== UTC 秒 → cm_tm_t（RTC 闹钟设置用）=====
 * cm_rtc_set_alarm 期望本地日历时间（官方 examples/alarm 示例：
 * cm_rtc_get_current_time() + cm_rtc_get_timezone()*3600 后再转日期），
 * 故转换前必须加时区偏移，否则闹钟落在本地过去 8 小时 */
static void utc_to_cm_tm(uint64_t utc_sec, cm_tm_t *out)
{
    time_t t = (time_t)(utc_sec + APP_TIMEZONE * 3600u);
    struct tm tmv;
    gmtime_r(&t, &tmv);
    out->tm_year = tmv.tm_year + 1900;
    out->tm_mon  = tmv.tm_mon + 1;
    out->tm_mday = tmv.tm_mday;
    out->tm_hour = tmv.tm_hour;
    out->tm_min  = tmv.tm_min;
    out->tm_sec  = tmv.tm_sec;
}

/* ===== 读取信号强度 ===== */
static int read_signal_strength(void)
{
    cm_radio_info_t info = {0};
    if (cm_modem_info_radio(&info) == 0) {
        /* SDK 换算公式 10*(rssi-111) 的输出单位是 0.1dBm（cm_modem_info.h 注释），
         * 实际 dBm = rssi - 111（实测 2026-08-20：原始 47 → -64dBm 合理）
         * rssi==0 表示模组未上报有效值，直接换算会得到 -111 dBm（无效） */
        if (info.rssi == 0) {
            return 0;
        }
        int rssi = (int)info.rssi - 111;
        return rssi;
    }
    return 0;
}

/* ===== 触发休眠模式下的单次定位+上报（供 app_command 调用） ===== */
void app_main_trigger_one_shot_location(void)
{
    g_one_shot_loc = true;
    /* 若处于 LP 睡眠，置事件唤醒主循环 */
    if (g_wake_evt) osEventFlagsSet(g_wake_evt, WAKE_EVT_NET);
}

/* ===== 离线缓存前把 is_offline_upload 置 true =====
 * 协议 5.1/7：实时点填 false，离线补传填 true。
 * 用等长替换（"false"→"true "，尾部空格为合法 JSON 空白），
 * 避免缓存后补传时 message_id/event_time 等字段偏移 */
static void mark_offline_upload(char *json)
{
    const char *pat = "\"is_offline_upload\":false";
    char *p = strstr(json, pat);
    if (p) {
        memcpy(p, "\"is_offline_upload\":true ", strlen(pat));
    }
}

/* ===== 上报一条定位（GPS 有效坐标 + 计步）===== */
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
        ",\"steps\":%lu"
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
        (unsigned long)g_steps,
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
        /* 标准版：离线缓存，待恢复连接后补传
         * 协议 5.1/7：缓存报文的 is_offline_upload 置 true */
        mark_offline_upload(s_json);
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

/* ===== 通用 telemetry 上报（供 app_lbs 等模块复用）=====
 * 在线直发；离线且长度不超缓存容量时进缓存（message_id/event_time 保持原值）
 * 返回 0=已发布，1=已缓存，<0=失败 */
int app_main_publish_telemetry(const char *json, int len,
                               const char *msgid, const char *event_time)
{
    if (!json || len <= 0 || !msgid || !event_time) return -1;

    if (app_mqtt_is_connected()) {
        app_mqtt_publish_telemetry(json, len);
        APP_LOGI("telemetry %s publish ok", msgid);
        return 0;
    }
    if (len >= (int)APP_OFFLINE_RECORD_PAYLOAD_LEN) {
        APP_LOGW("telemetry %s too long for cache, drop", msgid);
        return -2;
    }
    app_offline_record_t rec;
    memset(&rec, 0, sizeof(rec));
    strncpy(rec.message_id, msgid, sizeof(rec.message_id) - 1);
    strncpy(rec.event_time, event_time, sizeof(rec.event_time) - 1);
    memcpy(rec.payload, json, len);
    rec.payload[len] = '\0';
    rec.payload_len = len;
    app_offline_cache_push(&rec);
    APP_LOGI("telemetry %s cached (offline count=%d)",
             msgid, app_offline_cache_count());
    return 1;
}

/* ===== 上报状态 =====
 * 首次 ONLINE 附加设备信息（需求 6.7：app_ver/hw_ver/modem_ver/iccid）
 * 与复位原因（需求 9：异常复位事件上报平台）；
 * force_devinfo=true 时强制携带设备信息（GET_VERSION 平台查询应答） */
static void publish_state_ex(const char *online_status, bool force_devinfo)
{
    static char s_json[640];
    char ts[24];
    app_util_format_rfc3339(cm_rtc_get_current_time(), ts, sizeof(ts));

    const char *mode_str = app_mode_to_string(app_mode_get());

    int len;
    if (force_devinfo || !g_device_info_reported) {
        /* 首次上报：携带设备信息与复位原因（JSON 扩展字段，向后兼容） */
        len = snprintf(s_json, sizeof(s_json),
            "{\"event_type\":\"state\""
            ",\"message_id\":\"state_%s_%s\""
            ",\"imei\":\"%s\""
            ",\"device_sn\":\"%s\""
            ",\"online_status\":\"%s\""
            ",\"event_time\":\"%s\""
            ",\"mode\":\"%s\""
            ",\"battery_level\":%d"
            ",\"firmware_version\":\"%s\""
            ",\"hw_version\":\"%s\""
            ",\"modem_version\":\"%s\""
            ",\"iccid\":\"%s\""
            ",\"reset_reason\":%d"
            ",\"network_type\":\"LTE\""
            ",\"signal_strength\":%d"
            ",\"charging_status\":%d}",
            g_imei, ts, g_imei, g_imei, online_status, ts,
            mode_str ? mode_str : "supervise",
            g_last_soc, APP_FIRMWARE_VERSION, APP_HW_VERSION,
            g_modem_ver, g_iccid, g_reset_reason,
            g_last_rssi, g_charging_status);
        if (strcmp(online_status, APP_STATUS_ONLINE) == 0) {
            g_device_info_reported = true;
        }
    } else {
        len = snprintf(s_json, sizeof(s_json),
            "{\"event_type\":\"state\""
            ",\"message_id\":\"state_%s_%s\""
            ",\"imei\":\"%s\""
            ",\"device_sn\":\"%s\""
            ",\"online_status\":\"%s\""
            ",\"event_time\":\"%s\""
            ",\"mode\":\"%s\""
            ",\"battery_level\":%d"
            ",\"firmware_version\":\"%s\""
            ",\"network_type\":\"LTE\""
            ",\"signal_strength\":%d"
            ",\"charging_status\":%d}",
            g_imei, ts, g_imei, g_imei, online_status, ts,
            mode_str ? mode_str : "supervise",
            g_last_soc, APP_FIRMWARE_VERSION,
            g_last_rssi, g_charging_status);
    }

    if (len <= 0 || len >= (int)sizeof(s_json)) {
        APP_LOGE("state json build fail/overflow len=%d", len);
        return;
    }

    if (app_mqtt_is_connected()) {
        app_mqtt_publish_telemetry(s_json, len);
        APP_LOGI("state %s published", online_status);
    }
}

static void publish_state(const char *online_status)
{
    publish_state_ex(online_status, false);
}

/* ===== 平台 GET_VERSION 指令应答（需求 6.7：上报 app/hw/modem 版本等）===== */
void app_main_report_device_info(void)
{
    publish_state_ex(APP_STATUS_ONLINE, true);
}

/* ===== 平台 SHUTDOWN 指令（需求 V1.8）：OFFLINE 上报 → 断 MQTT → 断电关机 =====
 * 软件关机模式已删除：平台关机指令与长按 PWR_ON/OFF 硬件关机殊途同归 */
void app_main_execute_poweroff(void)
{
    APP_LOGW("execute poweroff (platform SHUTDOWN)");
    if (app_mqtt_is_connected()) {
        publish_state(APP_STATUS_OFFLINE);
        osDelay(APP_MS_TO_TICK(500));   /* 等 OFFLINE 报文发出 */
    }
    gps_power_close();
    app_mqtt_disconnect();
    cm_pm_poweroff();
    /* 正常不应返回；异常返回时记录日志（模组仍保持当前模式运行） */
    APP_LOGE("cm_pm_poweroff unexpected return");
}

/* ===== 离线补传回调 ===== */
static void offline_replay_cb(const app_offline_record_t *rec)
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

/* ===== MQTT 事件回调（cmmqtt-m 任务上下文，禁止调用 MQTT API / 阻塞） ===== */
static void mqtt_event_cb(app_mqtt_event_e evt, void *data)
{
    switch (evt) {
    case APP_MQTT_EVT_CONNECTED:
        g_mqtt_just_connected = true;
        break;
    case APP_MQTT_EVT_SUBSCRIBED:
        APP_LOGI("rpc subscribed");
        break;
    case APP_MQTT_EVT_DATA_RX: {
        /* 仅拷贝 payload 到静态缓冲区并置标志，由主循环处理 */
        app_mqtt_msg_t *msg = (app_mqtt_msg_t *)data;
        if (msg && msg->payload_len > 0 &&
            msg->payload_len < (int)sizeof(g_rpc_payload) && !g_rpc_pending) {
            strncpy(g_rpc_topic, msg->topic, sizeof(g_rpc_topic) - 1);
            g_rpc_topic[sizeof(g_rpc_topic) - 1] = '\0';
            memcpy(g_rpc_payload, msg->payload, msg->payload_len);
            g_rpc_payload_len = msg->payload_len;
            g_rpc_pending = true;
        }
        /* LP 睡眠期间网络下行（寻呼）即时唤醒主循环（需求 6.3） */
        if (g_wake_evt) osEventFlagsSet(g_wake_evt, WAKE_EVT_NET);
        break;
    }
    case APP_MQTT_EVT_DISCONNECTED:
        APP_LOGW("mqtt disconnected");
        break;
    default:
        break;
    }
}

/* ===== RTC 闹钟回调（LP 唤醒源之一；回调中禁止阻塞，仅置事件） ===== */
static void rtc_alarm_cb(void)
{
    if (g_wake_evt) osEventFlagsSet(g_wake_evt, WAKE_EVT_RTC);
}

/* ===== PM 低功耗回调（不可做耗时操作） ===== */
static void pm_enter_cb(void)
{
    /* 进入低功耗前回调，勿在此处做 IO 操作 */
}

static void pm_exit_cb(uint32_t reason)
{
    /* 任何 LP 唤醒（含 CHRG 边沿等 GPIO 唤醒）统一置事件，
     * 唤醒后的具体处理由主循环按周期项检查（不依赖 reason 数值） */
    (void)reason;
    if (g_wake_evt) osEventFlagsSet(g_wake_evt, WAKE_EVT_PM);
}

/* ===== NTP 事件回调（回调上下文，仅置标志） ===== */
static void ntp_event_cb(cm_ntp_event_e event, void *event_param, void *cb_param)
{
    (void)event_param; (void)cb_param;
    if (event == CM_NTP_EVENT_SYNC_OK) {
        g_ntp_synced = true;
        g_last_ntp_utc = cm_rtc_get_current_time();
        APP_LOGI("ntp sync ok");
    } else {
        APP_LOGW("ntp sync event=%d", (int)event);
    }
}

/* ===== 启动一次 NTP 对时（需求 6.5：MQTT 连接成功后 + 每天一次） ===== */
static void ntp_sync_start(void)
{
#if APP_NTP_ENABLE
    cm_ntp_set_cfg(CM_NTP_CFG_SERVER, (void *)APP_NTP_SERVER);
    uint32_t timeout = APP_NTP_TIMEOUT_MS;
    cm_ntp_set_cfg(CM_NTP_CFG_TIMEOUT, &timeout);
    cm_ntp_set_cfg(CM_NTP_CFG_CB, (void *)ntp_event_cb);
    if (cm_ntp_sync() != 0) {
        APP_LOGW("ntp sync start fail");
    }
#endif
}

/* ===== 计步：周期读取 + 0 点清零 + 静止判定（需求 8） =====
 * 每个定位周期调用一次。返回本周期是否处于静止状态。 */
static bool pedometer_cycle_update(void)
{
#if !APP_PEDOMETER_ENABLE
    return false;
#else
    uint32_t cnt = 0;
    if (bsp_pedometer_read(&cnt) != 0) {
        APP_LOGW("pedometer read fail");
        /* 传感器缺失/故障：保守复位静止判定——静止省电必须以真实步数为依据，
         * 宁可多耗电定位，不可因读数缺失误判静止而漏定位（丢宠物风险）。
         * （未焊接传感器时每周期走此分支，恒 false，定位正常） */
        g_still_cycles = 0;
        g_still_active = false;
        return false;
    }
    g_steps = cnt;

    /* 每天本地时间 0 点清零（需求 6.5：对时成功前不清零） */
    if (g_ntp_synced) {
        uint64_t local_sec = cm_rtc_get_current_time() + APP_TIMEZONE * 3600u;
        int day = (int)(local_sec / 86400u);
        if (g_last_clear_day < 0) {
            g_last_clear_day = day;   /* 首次仅记录基准，不清零 */
        } else if (day != g_last_clear_day) {
            g_last_clear_day = day;
            if (bsp_pedometer_clear() == 0) {
                g_steps = 0;
                g_last_step_cnt = 0;
                APP_LOGI("pedometer daily cleared (day=%d)", day);
            }
        }
    }

    /* 静止判定（需求 8 必做功能：计步增量法；APP_STILL_DETECT_ENABLE 仅调试关闭用） */
#if APP_STILL_DETECT_ENABLE
    /* 24bit 无符号差值：STEP_CLR 清零后回卷亦正确 */
    uint32_t delta = cnt - g_last_step_cnt;
    g_last_step_cnt = cnt;

    if (delta <= (uint32_t)APP_STILL_STEP_THRESHOLD) {
        if (g_still_cycles < APP_STILL_CYCLES) g_still_cycles++;
    } else {
        g_still_cycles = 0;
        g_still_active = false;
    }
    if (g_still_cycles >= APP_STILL_CYCLES && !g_still_active) {
        g_still_active = true;
        APP_LOGI("pet still detected (delta<=%d x %d cycles)",
                 APP_STILL_STEP_THRESHOLD, APP_STILL_CYCLES);
    }
#else
    g_last_step_cnt = cnt;
#endif
    return g_still_active;
#endif
}

/* ===== GPS 电源 + UART 开关（按模式统一管理）===== */
static int gps_power_open(void)
{
    if (g_gps_opened) return 0;
    bsp_gps_power_on();
    osDelay(APP_MS_TO_TICK(100));   /* GNSS 电源稳定 */
    if (bsp_gps_open(gps_rx_cb) != 0) {
        APP_LOGE("gps uart open fail");
        bsp_gps_power_off();
        return -1;
    }
    g_gps_opened = true;
    /* 重新上电后波特率对齐状态失效，需重新设置 CFGPRT（需求 2.1） */
    g_gps_baudrate_set = false;
    g_gps_lpmode_applied = -1;
    return 0;
}

static void gps_power_close(void)
{
    if (!g_gps_opened) return;
    bsp_gps_close();
    bsp_gps_power_off();
    g_gps_opened = false;
    g_gps_lpmode_applied = -1;
}

/* 工作模式 → GPS 功耗模式（ICOE CFGLPMODE）映射：
 * 寻宠 10s 高频次/快速移动        → 高性能（定位速度与轨迹优先）
 * 遛宠 30s 高动态                 → 自适应（保轨迹质量，软件自动降功耗）
 * 看护 5min / 省电 1h 长间隙定位   → 高性能（LP 流程中 GNSS 整体断电，
 *                                    间隙功耗由断电而非 CFGLPMODE 保障） */
static bsp_gps_lpmode_e gps_lpmode_for_app_mode(app_mode_e mode)
{
    switch (mode) {
    case APP_MODE_WALKING:
        return BSP_GPS_LPMODE_AUTO;
    default:                        /* SEARCHING/SUPERVISE/LOWPOWER：高性能 */
        return BSP_GPS_LPMODE_HIGH;
    }
}

/* ===== GPS 接收回调：解析并保存最新定位（bsp_gps_poll 主循环上下文调用） ===== */
static void gps_rx_cb(const char *line)
{
    app_location_t loc;
    memset(&loc, 0, sizeof(loc));
    if (bsp_gps_parse_nmea(line, &loc) == 1) {
        if (g_loc_mutex) osMutexAcquire(g_loc_mutex, osWaitForever);
        /* [FIX] RMC 语句无卫星数字段，解析结果 satellite_cnt=0；同秒 GGA 先到
         * 已更新卫星数，RMC 整体覆盖会将其清零，导致上报 satellite_cnt 恒为 0。
         * RMC 无卫星数时保留上次 GGA 的值 */
        if (loc.satellite_cnt == 0) {
            loc.satellite_cnt = g_last_loc.satellite_cnt;
        }
        g_last_loc = loc;
        if (g_loc_mutex) osMutexRelease(g_loc_mutex);
        g_loc_updated_tick = (uint32_t)osKernelGetTickCount();
        g_loc_updated_utc = cm_rtc_get_current_time();
    }
}

/* ===== GNSS 有效性判定（需求 2.1）=====
 * 上次有效定位时间距今超过当前模式定位周期的 2 倍 → GPS 无效。
 * 静止期间 GNSS 断电无新定位，但缓存坐标有效（静止判定本身证明未移动）。 */
static bool gnss_is_valid(uint32_t interval_ms)
{
    if (g_still_active) return true;   /* 静止期间缓存坐标有效 */
    if (g_loc_updated_utc == 0) return false;
    uint64_t now_utc = cm_rtc_get_current_time();
    uint64_t threshold_s = ((uint64_t)interval_ms / 1000u) * 2u;
    return (now_utc - g_loc_updated_utc) <= threshold_s;
}

/* ===== WiFi 扫描触发判定（需求 V1.12）=====
 * GNSS 连续 APP_LBS_WIFI_TRIGGER_INVALID_CYCLES 个定位周期无效时触发；
 * 看护/省电模式启用，寻宠/遛宠不触发；静止期间复用缓存 macs 不扫描。 */
static bool wifi_scan_should_trigger(app_mode_e mode)
{
    if (mode == APP_MODE_SEARCHING || mode == APP_MODE_WALKING) return false;
    if (g_still_active) return false;
    return g_gnss_invalid_cycles >= APP_LBS_WIFI_TRIGGER_INVALID_CYCLES;
}

/* ===== 本周期定位结果结算（WiFi 触发计数维护）===== */
static void gnss_cycle_settle(bool fixed_this_cycle)
{
    if (fixed_this_cycle) {
        g_gnss_invalid_cycles = 0;
    } else {
        g_gnss_invalid_cycles++;
    }
}

/* ===== 周期定位上报（活跃模式：寻宠/遛宠 GNSS 常开；LP 唤醒后亦复用）=====
 * LBS 参数每周期上报（需求 V1.2）；GPS 有效时另发 GPS 坐标报文 */
static void do_periodic_location_report(app_mode_e mode)
{
    uint32_t interval = app_mode_get_loc_interval_ms();

    /* 计步读取 + 静止判定（每个定位周期一次） */
    bool still = pedometer_cycle_update();
    app_lbs_set_steps(g_steps);

    /* GNSS 有效性：活跃模式本周期是否有新 fix（2 倍周期内有效定位） */
    bool valid = gnss_is_valid(interval);

    /* LBS 参数上报：无论 GNSS 是否有效（需求 V1.2）。
     * WiFi 扫描按触发策略（连续 N 周期无效 / 模式差异 / 静止复用） */
    bool scan_wifi = wifi_scan_should_trigger(mode);
    app_lbs_trigger(scan_wifi, true);

    if (valid) {
        publish_location(false);
    } else {
        APP_LOGI("gnss invalid this cycle (invalid_cycles=%d still=%d)",
                 g_gnss_invalid_cycles, (int)still);
    }

    /* 活跃模式（GNSS 常开）的周期结算：本周期有无新定位在 LP/唤醒流程结算，
     * 此处按有效性近似结算（valid=false 视为本周期无效） */
    gnss_cycle_settle(valid);
}

/* ===== LP 流程：设置 RTC 闹钟并进入睡眠等待（看护/省电模式）=====
 * step1: 按定位频率（或心跳到期，取更近者）设置 RTC 闹钟
 * step2: GNSS 断电、LED 熄灭（引脚睡眠态 pad 级已配置，进 LP 自动生效）
 * step3: 解锁睡眠锁，阻塞等待唤醒事件（RTC 闹钟 / 网络寻呼 / GPIO 边沿） */
static void lp_enter_sleep(app_mode_e mode)
{
    uint64_t now_utc = cm_rtc_get_current_time();

    /* 计算下次唤醒时刻：定位到期与心跳到期取近者（0 = 该项不调度，
     * 休眠模式仅有心跳唤醒，不安排定位） */
    uint64_t wake_utc = s_next_loc_due_utc;
    if (wake_utc == 0 || (s_next_hb_due_utc > 0 && s_next_hb_due_utc < wake_utc)) {
        wake_utc = s_next_hb_due_utc;
    }
    if (wake_utc == 0) wake_utc = now_utc + 60;   /* 防御：无调度项时 60s 兜底 */
    /* 最小闹钟间隔保护：过近时刻（<3s）的 RTC 闹钟因慢时钟域同步延迟
     * 存在错过永不触发的风险（2026-09-03 实测 wake in 1s 闹钟失效睡死），
     * 推迟到至少 3s 后 */
    if (wake_utc < (now_utc + APP_LP_ALARM_MIN_S)) {
        wake_utc = now_utc + APP_LP_ALARM_MIN_S;
    }

    cm_tm_t alarm;
    utc_to_cm_tm(wake_utc, &alarm);
    if (cm_rtc_set_alarm(&alarm) != 0) {
        APP_LOGE("lp: set alarm fail");
    }
    cm_rtc_enable_alarm(true);

    /* step2：GNSS 断电 + LED 熄灭（休眠/LP 睡眠，需求 5） */
    gps_power_close();
    bsp_led_stop();

    uint32_t sleep_s = (uint32_t)(wake_utc - now_utc);
    APP_LOGI("lp: enter sleep, wake in %lus (mode=%s)",
             (unsigned long)sleep_s, app_mode_to_string(mode));

    /* step3：清陈旧事件 → 解锁睡眠锁 → 阻塞等待唤醒。
     * cm_os.h 未导出 osEventFlagsClear，用 0 超时 Wait 排空已置位标志
     * （默认行为会清除取走的事件位；无事件时返回 Timeout，无副作用） */
    (void)osEventFlagsWait(g_wake_evt, WAKE_EVT_ALL, osFlagsWaitAny, 0);
    cm_pm_work_unlock();
    osEventFlagsWait(g_wake_evt, WAKE_EVT_ALL, osFlagsWaitAny, osWaitForever);
    cm_pm_work_lock();
    cm_rtc_enable_alarm(false);
    s_lp_slept_once = true;

    APP_LOGI("lp: wakeup");
}

/* ===== LP 流程 step4~6：唤醒后定位与上报 =====
 * step4: GNSS 上电 + 读取 LBS 缓存参数 + 静止判定（静止跳过 GNSS 开电）
 * step5: 等待 GNSS 定位完成，90s 超时判定无效（该等待在主循环非阻塞执行）
 * step6: 上报（GPS+LBS；GNSS 无效标记无效；满足条件执行 WiFi 扫描） */
static void lp_wakeup_handle(app_mode_e mode)
{
    uint64_t now_utc = cm_rtc_get_current_time();

    /* 心跳到期（省电 15min / 休眠 30min）：上报 state 维持 NAT 与 broker 会话 */
    if (s_next_hb_due_utc > 0 && now_utc >= s_next_hb_due_utc) {
        if (app_mqtt_is_connected()) {
            publish_state(APP_STATUS_ONLINE);
            APP_LOGI("lp: heartbeat state published");
        }
        uint32_t hb_ms = (mode == APP_MODE_SLEEP) ? APP_SLEEP_HEARTBEAT_MS
                                                  : APP_LOWPOWER_HEARTBEAT_MS;
        s_next_hb_due_utc = now_utc + hb_ms / 1000u;
    }

    /* 唤醒后若断网（弱信号/掉线）：踢醒重连（需求 6.1 退避由
     * app_mqtt_client 管理；LP 下达失败上限后由本唤醒周期触发重试）。
     * 仅真实 LP 唤醒后才检查：开机首轮 WOKE 时 MQTT 首连尚未完成，
     * 首连由 mqtt_task 自身负责，此处 kick 只会产生误报日志 */
    if (s_lp_slept_once && !app_mqtt_is_connected()) {
        APP_LOGW("lp: mqtt disconnected after wakeup, kick reconnect");
        app_mqtt_kick_reconnect();
    }

    /* 唤醒即强制检查充电状态（CHRG 边沿唤醒场景：主循环 2s 轮询门控
     * 在短唤醒窗口内不一定到期；变化即时上报，休眠充电自动恢复看护） */
    charge_poll();

    /* 定位未到期（寻呼/心跳唤醒）：回 LP 睡眠。
     * 到期判定加 APP_LP_DUE_GRACE_S 宽限：网络寻呼等外部唤醒常早于闹钟
     * 1~2s（RTC 整秒截断+唤醒延迟），若严格比较会因差 1s 回睡"过近闹钟"，
     * 2026-09-03 实测该场景 RTC 闹钟（1s 后）未触发导致睡死；
     * 差值在宽限内直接视为到期执行定位，同时省一次睡眠循环 */
    if (mode == APP_MODE_SLEEP) {
        return;   /* 休眠模式不主动定位（one-shot 由 RPC 触发） */
    }
    if ((now_utc + APP_LP_DUE_GRACE_S) < s_next_loc_due_utc) {
        return;
    }

    /* 定位周期到期：安排下周期 */
    uint32_t interval = app_mode_get_loc_interval_ms();
    s_next_loc_due_utc = now_utc + interval / 1000u;

    /* step4: 计步读取 + 静止判定（静止且 cellid 未变 → 跳过 GNSS 开电） */
    bool still = pedometer_cycle_update();
    app_lbs_set_steps(g_steps);

    /* LBS 安全网：cellid 变化（被动移动）立即恢复正常 GNSS 定位 */
    uint32_t cellid = 0;
    bool cellid_valid = (app_lbs_get_cached_cellid(&cellid) == 0);
    bool cellid_changed = cellid_valid && g_last_cellid_valid && (cellid != g_last_cellid);
    if (cellid_valid) {
        g_last_cellid = cellid;
        g_last_cellid_valid = true;
    }
    if (still && cellid_changed) {
        APP_LOGI("lp: cellid changed (passive move), resume gnss");
        g_still_active = false;
        g_still_cycles = 0;
        still = false;
    }

    if (still && APP_STILL_DETECT_ENABLE) {
        /* 静止：跳过 GNSS 定位（GNSS 保持断电），直接上报缓存坐标 + LBS */
        APP_LOGI("lp: still, skip gnss, report cached loc + lbs");
        app_lbs_trigger(false, true);   /* 静止期间复用缓存 macs，不扫描 */
        publish_location(false);        /* 缓存坐标（gnss_is_valid 静止返回 true） */
        gnss_cycle_settle(true);        /* 静止周期不计入 GNSS 无效 */
        return;
    }

    /* 非静止：GNSS 上电，进入定位等待（step5 由主循环非阻塞推进） */
    if (gps_power_open() != 0) {
        APP_LOGE("lp: gps power open fail, report lbs only");
        gnss_cycle_settle(false);
        app_lbs_trigger(wifi_scan_should_trigger(mode), true);
        return;
    }
    bsp_gps_set_power_mode(gps_lpmode_for_app_mode(mode));
    g_gps_lpmode_applied = (int)gps_lpmode_for_app_mode(mode);
    s_gnss_wait_start_tick = (uint32_t)osKernelGetTickCount();
    s_lp_state = LP_ST_GNSS_WAIT;
    APP_LOGI("lp: gnss on, waiting fix (timeout %dms)", APP_GNSS_FIX_TIMEOUT_MS);
}

/* ===== LP 主循环推进（看护/省电/休眠模式；每 20ms 调用）=====
 * 状态流转：SLEEP --唤醒--> WOKE --(心跳/定位/指令处理)--> GNSS_WAIT 或 SLEEP
 * WOKE 与 SLEEP 分离：唤醒后先回主循环跑完一轮（RPC/充电/电量等），
 * 确认无待办后才阻塞入睡（需求 step6：寻呼唤醒处理完指令再进 LP） */
static void lp_state_machine(app_mode_e mode)
{
    switch (s_lp_state) {
    case LP_ST_WOKE:
        /* 唤醒事务处理（心跳/定位到期）；定位则转 GNSS_WAIT（不入睡） */
        lp_wakeup_handle(mode);
        if (s_lp_state == LP_ST_WOKE) {
            s_lp_state = LP_ST_SLEEP;   /* 无定位任务：下一轮进入阻塞睡眠 */
        }
        break;

    case LP_ST_SLEEP:
        /* 阻塞睡眠直至唤醒事件（RTC 闹钟/网络寻呼/GPIO 边沿）；
         * 唤醒后转 WOKE，本轮先回主循环处理唤醒事务（RPC/充电） */
        lp_enter_sleep(mode);
        s_lp_state = LP_ST_WOKE;
        break;

    case LP_ST_GNSS_WAIT: {
        /* step5：等待定位（非阻塞）；fix 或 90s 超时 → step6 上报 */
        uint32_t now = (uint32_t)osKernelGetTickCount();
        bool fixed = ((now - g_loc_updated_tick) < APP_MS_TO_TICK(APP_GNSS_FIX_TIMEOUT_MS)) &&
                     (g_loc_updated_tick >= s_gnss_wait_start_tick);
        bool timeout = (now - s_gnss_wait_start_tick) >= APP_MS_TO_TICK(APP_GNSS_FIX_TIMEOUT_MS);
        if (fixed || timeout) {
            if (!fixed) {
                APP_LOGW("lp: gnss fix timeout %dms", APP_GNSS_FIX_TIMEOUT_MS);
            }
            /* 定位结束立即 GNSS 断电：后续 LBS 采集/WiFi 扫描窗口不再耗电 */
            gps_power_close();
            /* step6：LBS 每周期上报（含 WiFi 触发策略） + GPS 有效时坐标上报 */
            bool scan_wifi = wifi_scan_should_trigger(mode);
            app_lbs_trigger(scan_wifi, true);
            if (fixed) {
                publish_location(false);
            } else {
                APP_LOGI("lp: gnss invalid, report lbs only");
            }
            gnss_cycle_settle(fixed);
            /* 转 WOKE：LBS/WiFi 异步任务执行期间由主循环门控（app_lbs_is_running）
             * 阻止进入 lp_enter_sleep，主任务持有的睡眠锁不会释放，
             * 模组保持唤醒；任务结束后下轮入睡 */
            s_lp_state = LP_ST_WOKE;
        }
        break;
    }

    case LP_ST_IDLE:
    default:
        /* 进入 LP 模式首轮：看护/省电立即安排一次定位；休眠仅心跳唤醒 */
        if (mode != APP_MODE_SLEEP && s_next_loc_due_utc == 0) {
            s_next_loc_due_utc = cm_rtc_get_current_time();
        }
        s_lp_state = LP_ST_WOKE;
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

/* ===== Provisioning 流程（协议 3：HTTP Provisioning 获取 MQTT 凭证）===== */
static void provisioning_and_connect(void)
{
    app_mqtt_credential_t cred;
    memset(&cred, 0, sizeof(cred));

    wait_network_ready();

#if APP_USE_HARDCODED_CREDENTIAL
    /* === 联调阶段：直接使用硬编码凭证 === */
    strncpy(cred.mqtt_host, "119.23.217.155", sizeof(cred.mqtt_host) - 1);
    cred.mqtt_port = 18883;
    strncpy(cred.username, "yyf588r3y8ec6evxbv6e", sizeof(cred.username) - 1);
    cred.password[0] = '\0';
    strncpy(cred.client_id, g_imei, sizeof(cred.client_id) - 1);
    strncpy(cred.credential_type, "ACCESS_TOKEN", sizeof(cred.credential_type) - 1);
    APP_LOGI("use hardcoded credential (debug)");
    app_mqtt_connect(&cred);
#else
    /* === 正式流程：缓存凭证优先，否则走 SaaS provisioning === */
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
#endif
}

/* 供 WiFi 扫描（app_lbs.c）断开 MQTT 扫描后重连使用：
 * 复用完整凭证流程（硬编码/缓存/provisioning 均覆盖） */
void app_reconnect_mqtt(void)
{
    provisioning_and_connect();
}

/* ===== 休眠模式单次定位任务（独立线程，避免阻塞主任务） ===== */
static void one_shot_loc_task(void *arg)
{
    (void)arg;
    APP_LOGI("one-shot: open gps");
    g_one_shot_running = true;

    uint64_t utc_before_open = g_loc_updated_utc;

    if (gps_power_open() != 0) {
        APP_LOGE("one-shot: gps open fail, fallback to LBS");
        app_lbs_trigger(false, true);
        g_one_shot_running = false;
        return;
    }
    bsp_gps_set_power_mode(BSP_GPS_LPMODE_HIGH);
    g_gps_lpmode_applied = (int)BSP_GPS_LPMODE_HIGH;

    /* 等待 GPS 获取有效定位（带超时） */
    uint32_t start = (uint32_t)osKernelGetTickCount();
    osDelay(APP_MS_TO_TICK(3000));   /* GNSS 上电稳定 */
    while (1) {
        uint32_t elapsed = (uint32_t)osKernelGetTickCount() - start;
        if (elapsed >= APP_MS_TO_TICK(APP_GNSS_FIX_TIMEOUT_MS)) {
            APP_LOGW("one-shot: timeout %lums", (unsigned long)(elapsed * APP_TICK_MS));
            break;
        }
        if (g_loc_updated_utc != utc_before_open) {
            APP_LOGI("one-shot: location fixed after %lums",
                     (unsigned long)(elapsed * APP_TICK_MS));
            break;
        }
        osDelay(APP_MS_TO_TICK(500));
    }

    /* 需求 1（休眠模式注 3）：定位完成之后关闭定位功能 */
    gps_power_close();

    /* 上报一次数据：GPS 有效定位 → 上报定位；
     * 定位失败 → LBS 原始参数兜底（平台调高德解算） */
    bool fixed = (g_loc_updated_utc != utc_before_open);
    pedometer_cycle_update();
    app_lbs_set_steps(g_steps);
    app_lbs_trigger(false, true);
    if (fixed) {
        publish_location(false);
    }

    g_one_shot_running = false;
    APP_LOGI("one-shot: task done");
}

/* ===== 应用层日志输出（通过 DBG 口） ===== */
/* 日志时间戳：NTP 对时成功后用本地时间（UTC+8，与 RTC 闹钟约定一致，
 * 含日期，跨零点后日志可区分天；对时前 RTC 无效，改用开机后秒数（前缀 '+' 区分）。
 * 日期换算复用 utc_to_cm_tm 的 gmtime_r 方式（UTC+偏移后按 UTC 解析，不依赖 tz） */
static void app_log_timestamp(char *out, size_t size)
{
    if (g_ntp_synced) {
        time_t t = (time_t)(cm_rtc_get_current_time() + (uint64_t)APP_TIMEZONE * 3600u);
        struct tm tmv;
        (void)gmtime_r(&t, &tmv);
        /* struct tm 字段实际有界（tm_hour≤23、tm_year 为年份-1900），但编译器无法
         * 感知 gmtime_r 的输出范围，按 int 全范围评估会误报 -Wformat-truncation；
         * 钳位后值域不变（如 x%100 对 0~59 恒等），仅供编译器推断输出长度 */
        (void)snprintf(out, size, "%04u-%02u-%02u %02u:%02u:%02u",
                       (unsigned)((tmv.tm_year + 1900) % 10000u),
                       (unsigned)((tmv.tm_mon + 1) % 100u),
                       (unsigned)(tmv.tm_mday % 100u),
                       (unsigned)(tmv.tm_hour % 100u),
                       (unsigned)(tmv.tm_min % 100u),
                       (unsigned)(tmv.tm_sec % 100u));
    } else {
        uint32_t sec = osKernelGetTickCount() * APP_TICK_MS / 1000u;
        (void)snprintf(out, size, "+%lus", (unsigned long)sec);
    }
}

void app_log_output(const char *level, const char *fmt, ...)
{
    char ts[24];
    app_log_timestamp(ts, sizeof(ts));
    char buf[256];
    int len = snprintf(buf, sizeof(buf), APP_LOG_TAG "[%s][%s] ", level, ts);
    if (len < 0 || len >= (int)sizeof(buf)) return;

    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf + len, sizeof(buf) - len, fmt, args);
    va_end(args);
    if (n < 0) return;
    len += n;
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

/* ===== 充电检测（需求 7：CHRG_State 轮询 + 变化即时上报 + 休眠恢复看护）===== */
static void charge_poll(void)
{
    int now_charging = bsp_charge_is_charging() ? 1 : 0;
    if (now_charging == g_charging_status) return;

    g_charging_status = now_charging;
    APP_LOGI("charging status -> %d, report state", now_charging);
    if (app_mqtt_is_connected()) {
        publish_state(APP_STATUS_ONLINE);
    }
    /* 休眠模式下检测到充电：自动恢复看护模式（需求 7） */
    if (now_charging && app_mode_get() == APP_MODE_SLEEP) {
        APP_LOGI("charging in sleep mode, resume supervise");
        app_mode_set(APP_MODE_SUPERVISE);
    }
}

/* ===== LED 常态指示维护（需求 5）=====
 * 低电量双闪 > 未联网快闪 > 已联网慢闪；LP 睡眠前熄灭（lp_enter_sleep 处理） */
static void led_status_poll(app_mode_e mode)
{
    static bsp_led_pattern_e s_cur = BSP_LED_PATTERN_OFF;
    bsp_led_pattern_e want;

    if (mode == APP_MODE_SLEEP) {
        want = BSP_LED_PATTERN_OFF;            /* 休眠：熄灭 */
    } else if (g_last_soc >= 0 && g_last_soc < APP_LOW_BATTERY_THRESHOLD) {
        want = BSP_LED_PATTERN_LOW_BATTERY;    /* 低电量：每秒双闪 */
    } else if (!app_mqtt_is_connected()) {
        want = BSP_LED_PATTERN_OFFLINE;        /* 未联网：快闪 5Hz */
    } else {
        want = BSP_LED_PATTERN_ONLINE;         /* 已联网：慢闪每 3 秒 */
    }

    if (want != s_cur) {
        s_cur = want;
        bsp_led_set_pattern(want);
    }
}

/* ===== 主业务任务 ===== */
static void main_task(void *arg)
{
    (void)arg;

    /* 需求 V1.8：删除软件关机模式。上电即工作，进入 flash 恢复的模式
     * （app_mode_init 已完成恢复；无有效保存时默认看护模式），自动联网 */
    APP_LOGI("boot -> mode=%s, connecting", app_mode_to_string(app_mode_get()));
    /* MQTT keepalive 按模式区分（需求 6.3），首连即生效 */
    app_mode_e boot_mode = app_mode_get();
    app_mqtt_set_keepalive_sec(
        (boot_mode == APP_MODE_SEARCHING || boot_mode == APP_MODE_WALKING)
            ? APP_MQTT_KEEPALIVE_HIGHFREQ_SEC : APP_MQTT_KEEPALIVE_LP_SEC);
    provisioning_and_connect();

    uint32_t last_rssi_tick = 0;
    uint32_t last_replay_tick = 0;
    uint32_t last_charge_tick = 0;
    uint32_t last_led_tick = 0;
#if APP_BATTERY_ENABLE
    /* 电量相关局部状态：随 APP_BATTERY_ENABLE 一起裁剪，避免调试期
     * （无电池，宏=0）出现 unused variable 告警 */
    uint32_t last_battery_tick = 0;
    bool low_battery_announced = false;
    bool ultra_low_battery_announced = false;
    uint8_t s_ultra_low_confirm = 0;    /* 超低电连续确认计数 */
#endif
    app_mode_e last_mode = APP_MODE_NUM;
    app_mode_e last_state_mode = APP_MODE_NUM;

    while (1) {
        /* 轮询 GPS UART 接收数据（替代 RX 中断回调，避免中断上下文
         * 调用 APP_LOG/osMutex 触发 OSA tx 重入静默复位） */
        bsp_gps_poll();

        app_mode_e mode = app_mode_get();
        uint32_t interval = app_mode_get_loc_interval_ms();
        uint32_t now = (uint32_t)osKernelGetTickCount();
        uint64_t now_utc = cm_rtc_get_current_time();

        /* MQTT 连接成功后执行 subscribe + ONLINE + 设备信息 + NTP 对时 */
        if (g_mqtt_just_connected) {
            g_mqtt_just_connected = false;
            app_mqtt_subscribe_rpc();
            publish_state(APP_STATUS_ONLINE);
            /* 需求 6.5：MQTT 连接成功后对时（此后每天一次，见下方周期检查） */
            if (!g_ntp_synced) {
                ntp_sync_start();
            }
            /* 启动增量离线补传 */
            g_offline_replay_active = (app_offline_cache_count() > 0);
            if (g_offline_replay_active) {
                APP_LOGI("offline replay start, count=%d", app_offline_cache_count());
            }
        }

        /* 需求 6.5：每天对时一次 */
        if (g_ntp_synced && app_mqtt_is_connected() &&
            (now_utc - g_last_ntp_utc) >= (APP_NTP_RESYNC_INTERVAL_MS / 1000u)) {
            g_last_ntp_utc = now_utc;
            ntp_sync_start();
        }

        /* 增量离线补传：协议 7 限速最多 5 条/秒 */
        if (g_offline_replay_active) {
            if (!app_mqtt_is_connected()) {
                g_offline_replay_active = false;
                APP_LOGW("offline replay paused (mqtt disconnected)");
            } else if ((now - last_replay_tick) >= APP_MS_TO_TICK(1000 / APP_OFFLINE_REPLAY_RATE)) {
                last_replay_tick = now;
                int r = app_offline_cache_replay_step(offline_replay_cb);
                if (r != 0) {
                    g_offline_replay_active = false;
                    APP_LOGI("offline replay done");
                }
            }
        }

        /* 处理 MQTT 回调暂存的 RPC 消息 */
        if (g_rpc_pending) {
            app_command_handle(g_rpc_topic, g_rpc_payload, g_rpc_payload_len);
            g_rpc_pending = false;
        }

        /* 模式变化上报状态事件（协议 5.2：mode 必填字段） */
        if (mode != last_state_mode) {
            bool first_change = (last_state_mode == APP_MODE_NUM);
            last_state_mode = mode;
            if (!first_change && app_mqtt_is_connected()) {
                APP_LOGI("mode changed -> %s, report state", app_mode_to_string(mode));
                publish_state(APP_STATUS_ONLINE);
            }
        }

        /* 模式切换时的资源调度（LP 状态机边界） */
        if (mode != last_mode) {
            last_mode = mode;
            /* MQTT keepalive 按模式区分（需求 6.3），下次（重）连接生效 */
            app_mqtt_set_keepalive_sec(
                (mode == APP_MODE_SEARCHING || mode == APP_MODE_WALKING)
                    ? APP_MQTT_KEEPALIVE_HIGHFREQ_SEC : APP_MQTT_KEEPALIVE_LP_SEC);
            if (mode == APP_MODE_SEARCHING || mode == APP_MODE_WALKING) {
                /* 退出 LP：寻宠/遛宠 GNSS 常开（热启动，定位周期短于冷启动） */
                s_lp_state = LP_ST_IDLE;
                s_next_loc_due_utc = 0;
                s_next_hb_due_utc = 0;
                if (!g_one_shot_running && gps_power_open() == 0) {
                    bsp_gps_lpmode_e lpm = gps_lpmode_for_app_mode(mode);
                    bsp_gps_set_power_mode(lpm);
                    g_gps_lpmode_applied = (int)lpm;
                }
            } else {
                /* 看护/省电/休眠：进入 LP 调度（RTC 闹钟 + 寻呼唤醒）。
                 * 看护/省电：立即安排一轮定位；休眠：仅 30 分钟心跳，GNSS 常关 */
                s_lp_state = LP_ST_IDLE;
                s_next_loc_due_utc = (mode == APP_MODE_SLEEP) ? 0 : now_utc;
                s_next_hb_due_utc = (mode == APP_MODE_LOWPOWER)
                                    ? now_utc + APP_LOWPOWER_HEARTBEAT_MS / 1000u
                                  : (mode == APP_MODE_SLEEP)
                                    ? now_utc + APP_SLEEP_HEARTBEAT_MS / 1000u : 0;
                if (mode == APP_MODE_SLEEP && !g_one_shot_running) {
                    gps_power_close();
                }
            }
        }

        /* 需求 1：寻宠 10 分钟 / 遛宠 30 分钟超时自动切回看护模式 */
        if (app_mode_auto_switch_expired()) {
            APP_LOGI("mode timeout -> supervise");
            continue;
        }

        /* 电量周期采样 + 阈值跨越状态事件 + 超低电强制休眠（需求 7）
         * [FIX] 采样节流：cm_adc_read 经由 modem OSA tx 路径，不可高频调用 */
#if APP_BATTERY_ENABLE
        if ((now - last_battery_tick) >= APP_MS_TO_TICK(APP_BATTERY_SAMPLE_MS)) {
            last_battery_tick = now;
            int mv = 0, soc = -1;
            if (bsp_battery_read(&mv, &soc) == 0) {
                if (soc != g_last_soc) {
                    APP_LOGI("battery mv=%d soc=%d", mv, soc);
                    g_last_soc = soc;
                }
                if (soc >= 0) {
                    if (soc < APP_SUPER_LOW_BATTERY) {
                        /* 超低电连续确认（硬件定版：仅外部分压 ADC，无 VBAT
                         * 交叉校验；连续 APP_BATTERY_ULTRA_LOW_CONFIRM 次采样
                         * 低于阈值才动作，防单次误读强制休眠——2026-09-03
                         * 曾因引脚误配读到假 2408mV 被误切休眠） */
                        if (s_ultra_low_confirm < APP_BATTERY_ULTRA_LOW_CONFIRM) {
                            s_ultra_low_confirm++;
                            APP_LOGW("ultra low battery pending (soc=%d, %d/%d)",
                                     soc, s_ultra_low_confirm, APP_BATTERY_ULTRA_LOW_CONFIRM);
                        }
                        if (s_ultra_low_confirm >= APP_BATTERY_ULTRA_LOW_CONFIRM) {
                            /* 超低电量：上报一次状态事件后强制切换到休眠模式 */
                            if (!ultra_low_battery_announced) {
                                ultra_low_battery_announced = true;
                                low_battery_announced = true;
                                APP_LOGW("ultra low battery soc=%d confirmed, report state + force sleep", soc);
                                if (app_mqtt_is_connected()) {
                                    publish_state(APP_STATUS_ONLINE);
                                    osDelay(APP_MS_TO_TICK(200));
                                }
                            }
                            if (mode != APP_MODE_SLEEP) {
                                app_mode_set(APP_MODE_SLEEP);
                            }
                        }
                    } else if (soc < APP_LOW_BATTERY_THRESHOLD) {
                        s_ultra_low_confirm = 0;
                        if (!low_battery_announced) {
                            low_battery_announced = true;
                            APP_LOGW("low battery soc=%d, report state", soc);
                            publish_state(APP_STATUS_ONLINE);
                        }
                    } else {
                        low_battery_announced = false;
                        ultra_low_battery_announced = false;
                    }
                    /* 休眠模式 SOC 回升至阈值以上自动恢复看护（需求 7） */
                    if (mode == APP_MODE_SLEEP && soc >= APP_BATTERY_RECOVER_SOC) {
                        APP_LOGI("battery recovered soc=%d, resume supervise", soc);
                        app_mode_set(APP_MODE_SUPERVISE);
                    }
                }
            }
        }
#endif /* APP_BATTERY_ENABLE */

        /* 充电状态轮询（2 秒；变化即时上报，休眠充电恢复看护） */
        if ((now - last_charge_tick) >= APP_MS_TO_TICK(2000)) {
            last_charge_tick = now;
            charge_poll();
        }

        /* LED 常态指示维护（2 秒检查；LP 模式进睡眠前由 lp_enter_sleep 熄灭） */
        if ((now - last_led_tick) >= APP_MS_TO_TICK(2000) &&
            s_lp_state != LP_ST_SLEEP) {
            last_led_tick = now;
            led_status_poll(mode);
        }

        /* 信号强度周期采样（WiFi 扫描静默窗口内跳过，避免 AT 扰动天线仲裁） */
        if ((now - last_rssi_tick) >= APP_MS_TO_TICK(5000) &&
            !app_lbs_is_modem_quiet()) {
            last_rssi_tick = now;
            g_last_rssi = read_signal_strength();
        }

        /* ===== 分模式定位调度 ===== */
        if (mode == APP_MODE_SUPERVISE || mode == APP_MODE_LOWPOWER ||
            mode == APP_MODE_SLEEP) {
            /* 看护/省电：LP 完整流程（定位+心跳）；
             * 休眠：LP 心跳调度（GNSS 常关，one-shot 由 RPC 触发）。
             * one-shot / LBS-WiFi 任务运行期间暂停 LP 推进保持唤醒 */
            if (!g_one_shot_running && !app_lbs_is_running()) {
                lp_state_machine(mode);
            }
        } else if (mode == APP_MODE_SEARCHING || mode == APP_MODE_WALKING) {
            /* 寻宠/遛宠：GNSS 常开，按周期上报（LBS 每周期 + GPS 有效时坐标） */
            if (interval > 0 && (now_utc >= s_next_loc_due_utc) &&
                !app_lbs_is_modem_quiet()) {
                s_next_loc_due_utc = now_utc + interval / 1000u;
                do_periodic_location_report(mode);
            }
        }

        /* 休眠模式下单次定位触发（收到平台状态读取/定位指令后） */
        if (g_one_shot_loc) {
            g_one_shot_loc = false;
            APP_LOGI("one-shot location triggered");
            osThreadAttr_t attr = {0};
            attr.name = "one_shot";
            attr.stack_size = 4096;
            attr.priority = osPriorityBelowNormal1;
            osThreadId_t tid = osThreadNew(one_shot_loc_task, NULL, &attr);
            if (tid == NULL) {
                APP_LOGE("one-shot task create fail, fallback direct publish");
                publish_location(false);
            }
        }

        /* 周期心跳日志，确认程序正常运行 */
        static uint32_t last_hb_log_tick = 0;
        if (now - last_hb_log_tick >= APP_MS_TO_TICK(10000)) {
            last_hb_log_tick = now;
            app_location_t hb_loc;
            if (g_loc_mutex) osMutexAcquire(g_loc_mutex, osWaitForever);
            hb_loc = g_last_loc;
            if (g_loc_mutex) osMutexRelease(g_loc_mutex);
            const char *mode_name = app_mode_to_string(mode);
            const char *bts = app_lbs_get_cached_bts();
            APP_LOGI("alive mode=%s rssi=%d chg=%d steps=%lu gps=%s sat=%d lat=%.5f lon=%.5f lbs=%s",
                     mode_name ? mode_name : "?", g_last_rssi, g_charging_status,
                     (unsigned long)g_steps,
                     (hb_loc.latitude != 0.0 || hb_loc.longitude != 0.0) ? "FIX" : "NOFIX",
                     hb_loc.satellite_cnt, hb_loc.latitude, hb_loc.longitude,
                     bts ? bts : "none");
        }

        osDelay(APP_MS_TO_TICK(20));
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

    /* 唤醒事件（LP 睡眠等待） */
    g_wake_evt = osEventFlagsNew(NULL);

    app_mode_init();          /* 需求 9：从 flash 恢复掉电前模式，首次默认看护 */
    app_offline_cache_init();
    app_mqtt_init(mqtt_event_cb);

    /* PM 初始化 + 睡眠锁：boot 后保持唤醒，LP 由主循环按模式管理
     * （进 LP 时 work_unlock，唤醒后 work_lock） */
    cm_pm_cfg_t pm_cfg = { pm_enter_cb, pm_exit_cb };
    cm_pm_init(pm_cfg);
    cm_pm_work_lock();

    /* RTC 闹钟回调（LP 唤醒源）；时区固定 UTC+8（需求 6.5） */
    cm_rtc_register_alarm_cb(rtc_alarm_cb);
    cm_rtc_set_timezone(APP_TIMEZONE);

    /* 需求 9：读取复位原因，随首次 ONLINE 状态上报平台 */
    g_reset_reason = cm_pm_get_power_on_reason();
    APP_LOGI("power on reason=%d", g_reset_reason);

    if (cm_sys_get_imei(g_imei) != 0) {
        strcpy(g_imei, "000000000000000");
    }
    app_command_set_imei(g_imei);
    app_lbs_set_imei(g_imei);

    /* 需求 6.7：设备信息（modem 基带版本 / ICCID），随首次 ONLINE 上报 */
    if (cm_sys_get_cm_ver(g_modem_ver, sizeof(g_modem_ver)) < 0) {
        g_modem_ver[0] = '\0';
    }
    if (cm_sim_get_iccid(g_iccid) != 0) {
        g_iccid[0] = '\0';
    }

    /* 每次启动生成新 boot_id 并清零序号（RAM-only，不落盘：
     * boot_id+seq 仅用于云端幂等去重，掉电丢失无影响） */
    app_util_gen_boot_id(g_boot_id, sizeof(g_boot_id));
    g_seq = 0;
    APP_LOGI("IMEI=%s ver=%s hw=%s modem=%s boot_id=%s",
             g_imei, APP_FIRMWARE_VERSION, APP_HW_VERSION, g_modem_ver, g_boot_id);
    app_lbs_set_boot_id(g_boot_id);

    bsp_init();
    app_lbs_init();

    /* 充电检测脚 LP 边沿唤醒（需求 7：LP 睡眠期间插入充电即时唤醒处理）。
     * PINCMD1 边沿检测提供唤醒事件（唤醒后由 pm_exit_cb 置事件标志，
     * 主循环 charge_poll 读取最新电平上报）；双边沿覆盖插入/拔出。 */
    cm_iomux_set_pin_cmd(APP_CHRG_IOMUX_PIN, CM_IOMUX_PINCMD1_LPMEDEG,
                         CM_IOMUX_PINCMD1_FUNC3_EDGE_BOTH);
}

/* ====================================================================
 * OpenCPU 入口
 * ==================================================================== */
int cm_opencpu_entry(void *param)
{
    (void)param;

    system_init();

    /* 需求 V1.8：上电即工作（无软件关机模式；开关机由 PWR_ON/OFF 硬件控制） */
    APP_LOGI("pet tracker boot, mode=%s", app_mode_to_string(app_mode_get()));

    osThreadAttr_t task_attr = {0};
    task_attr.name = "pet_main";
    task_attr.stack_size = 12 * 1024;
    task_attr.priority = osPriorityNormal1;
    if (osThreadNew(main_task, NULL, &task_attr) == NULL) {
        APP_LOGE("main task create fail");
        return -1;
    }

    return 0;
}
