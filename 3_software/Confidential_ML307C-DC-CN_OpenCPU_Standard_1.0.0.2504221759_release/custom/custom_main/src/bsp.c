/**
 * @file    bsp.c
 * @brief   板级驱动实现
 *          - 按键：中断+轮询去抖+长按 5 秒
 *          - 蜂鸣器/RGB：GPIO 控制
 *          - 电池：ADC 采样
 *          - GPS：UART + NMEA 0183 解析（GGA / RMC / GSV）
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "cm_os.h"
#include "cm_sys.h"
#include "cm_iomux.h"
#include "cm_gpio.h"
#include "cm_uart.h"
#include "cm_adc.h"
#include "cm_pm.h"
#include "app_log.h"
#include "app_config.h"
#include "bsp.h"

/* ====================================================================
 * 按键
 * ==================================================================== */
static bsp_key_event_cb_t s_key_cb = NULL;
static osTimerId_t        s_key_timer = NULL;
static uint32_t           s_key_press_tick = 0;
static bool               s_key_pressed = false;
static volatile bool      s_key_irq_flag = false;

static void key_irq_handler(void)
{
    /* 中断上下文，仅置位标志，由 main task 处理去抖 */
    s_key_irq_flag = true;
}

static void key_check_timer_cb(void *arg)
{
    (void)arg;
    cm_gpio_level_e level = CM_GPIO_LEVEL_LOW;
    cm_gpio_get_level(APP_KEY_GPIO, &level);

    bool pressed_now = (level == APP_KEY_ACTIVE_LEVEL);

    if (pressed_now && !s_key_pressed) {
        /* 按下沿 */
        s_key_pressed = true;
        s_key_press_tick = (uint32_t)osKernelGetTickCount();
    } else if (!pressed_now && s_key_pressed) {
        /* 释放沿 */
        uint32_t held = (uint32_t)osKernelGetTickCount() - s_key_press_tick;
        s_key_pressed = false;
        if (held >= APP_MS_TO_TICK(APP_KEY_LONGPRESS_MS)) {
            if (s_key_cb) s_key_cb(true);
        } else if (held > APP_MS_TO_TICK(50)) {
            if (s_key_cb) s_key_cb(false);
        }
    }
}

static int bsp_key_init(void)
{
    cm_iomux_set_pin_func(APP_KEY_IOMUX_PIN, CM_IOMUX_FUNC_FUNCTION1); /* GPIO 功能 */
    cm_gpio_cfg_t cfg = {0};
    cfg.direction = CM_GPIO_DIRECTION_INPUT;
    cfg.pull = (APP_KEY_ACTIVE_LEVEL == CM_GPIO_LEVEL_LOW) ? CM_GPIO_PULL_UP : CM_GPIO_PULL_DOWN;
    if (cm_gpio_init(APP_KEY_GPIO, &cfg) != 0) {
        APP_LOGE("key gpio init fail");
        return -1;
    }
    cm_gpio_interrupt_register(APP_KEY_GPIO, (void *)key_irq_handler);
    cm_gpio_interrupt_enable(APP_KEY_GPIO, APP_KEY_INTERRUPT_MODE);

    if (s_key_timer == NULL) {
        osTimerAttr_t tattr = {0};
        tattr.name = "key_t";
        s_key_timer = osTimerNew(key_check_timer_cb, osTimerPeriodic, NULL, &tattr);
        osTimerStart(s_key_timer, APP_MS_TO_TICK(20)); /* 20ms 周期去抖 */
    }
    return 0;
}

void bsp_key_register_cb(bsp_key_event_cb_t cb)
{
    s_key_cb = cb;
}

bool bsp_key_is_pressed(void)
{
    return s_key_pressed;
}

/* ====================================================================
 * 蜂鸣器
 * ==================================================================== */
static int bsp_buzzer_init(void)
{
    cm_iomux_set_pin_func(APP_BUZZER_IOMUX_PIN, CM_IOMUX_FUNC_FUNCTION1);
    cm_gpio_cfg_t cfg = {0};
    cfg.direction = CM_GPIO_DIRECTION_OUTPUT;
    cfg.pull = CM_GPIO_PULL_NONE;
    if (cm_gpio_init(APP_BUZZER_GPIO, &cfg) != 0) return -1;
    cm_gpio_set_level(APP_BUZZER_GPIO,
                      (APP_BUZZER_ACTIVE_LEVEL == CM_GPIO_LEVEL_HIGH) ? CM_GPIO_LEVEL_LOW : CM_GPIO_LEVEL_HIGH);
    return 0;
}

int bsp_buzzer_on(void)
{
    return cm_gpio_set_level(APP_BUZZER_GPIO, APP_BUZZER_ACTIVE_LEVEL);
}

int bsp_buzzer_off(void)
{
    return cm_gpio_set_level(APP_BUZZER_GPIO,
                             (APP_BUZZER_ACTIVE_LEVEL == CM_GPIO_LEVEL_HIGH) ? CM_GPIO_LEVEL_LOW : CM_GPIO_LEVEL_HIGH);
}

void bsp_buzzer_beep(int times, uint32_t on_ms, uint32_t off_ms)
{
    for (int i = 0; i < times; i++) {
        bsp_buzzer_on();
        osDelay(APP_MS_TO_TICK(on_ms));
        bsp_buzzer_off();
        if (i + 1 < times) osDelay(APP_MS_TO_TICK(off_ms));
    }
}

/* 异步持续响铃：osTimer 周期触发，每秒响一次 */
typedef struct {
    osTimerId_t timer;
    uint32_t    on_ms;
    uint32_t    off_ms;
    uint32_t    period_ms;
    uint32_t    start_tick;
    uint32_t    duration_ms;   /* 0 = 持续 */
    bool        running;
    bool        on_phase;
} buzzer_async_ctx_t;

static buzzer_async_ctx_t s_buzz_ctx = {0};

static void buzzer_async_cb(void *arg)
{
    (void)arg;
    if (!s_buzz_ctx.running) return;
    /* 检查总时长 */
    if (s_buzz_ctx.duration_ms > 0) {
        uint32_t elapsed = (uint32_t)osKernelGetTickCount() - s_buzz_ctx.start_tick;
        if (elapsed >= APP_MS_TO_TICK(s_buzz_ctx.duration_ms)) {
            bsp_buzzer_off();
            s_buzz_ctx.running = false;
            return;
        }
    }
    /* 切换响/停相位 */
    s_buzz_ctx.on_phase = !s_buzz_ctx.on_phase;
    if (s_buzz_ctx.on_phase) {
        bsp_buzzer_on();
        osTimerStart(s_buzz_ctx.timer, APP_MS_TO_TICK(s_buzz_ctx.on_ms));
    } else {
        bsp_buzzer_off();
        osTimerStart(s_buzz_ctx.timer, APP_MS_TO_TICK(s_buzz_ctx.off_ms));
    }
}

void bsp_buzzer_beep_async(uint32_t duration_sec)
{
    /* 停止之前的异步响铃 */
    if (s_buzz_ctx.timer && s_buzz_ctx.running) {
        osTimerStop(s_buzz_ctx.timer);
        bsp_buzzer_off();
    }
    if (s_buzz_ctx.timer == NULL) {
        osTimerAttr_t tattr = {0};
        tattr.name = "buz_async";
        s_buzz_ctx.timer = osTimerNew(buzzer_async_cb, osTimerOnce, NULL, &tattr);
        if (s_buzz_ctx.timer == NULL) return;
    }
    s_buzz_ctx.on_ms      = APP_BUZZER_BEEP_ON_MS;
    s_buzz_ctx.off_ms     = APP_BUZZER_BEEP_OFF_MS;
    s_buzz_ctx.duration_ms = duration_sec * 1000;
    s_buzz_ctx.start_tick = (uint32_t)osKernelGetTickCount();
    s_buzz_ctx.on_phase   = false;
    s_buzz_ctx.running    = true;
    /* 立即响一次 */
    bsp_buzzer_on();
    s_buzz_ctx.on_phase = true;
    osTimerStart(s_buzz_ctx.timer, APP_MS_TO_TICK(s_buzz_ctx.on_ms));
}

void bsp_buzzer_stop(void)
{
    if (s_buzz_ctx.timer && s_buzz_ctx.running) {
        osTimerStop(s_buzz_ctx.timer);
    }
    bsp_buzzer_off();
    s_buzz_ctx.running = false;
}

/* ====================================================================
 * RGB LED
 * ==================================================================== */
static int bsp_rgb_init_one(cm_gpio_num_e gpio, cm_iomux_pin_e pin)
{
    cm_iomux_set_pin_func(pin, CM_IOMUX_FUNC_FUNCTION1);
    cm_gpio_cfg_t cfg = {0};
    cfg.direction = CM_GPIO_DIRECTION_OUTPUT;
    cfg.pull = CM_GPIO_PULL_NONE;
    if (cm_gpio_init(gpio, &cfg) != 0) return -1;
    cm_gpio_set_level(gpio,
                      (APP_RGB_ACTIVE_LEVEL == CM_GPIO_LEVEL_HIGH) ? CM_GPIO_LEVEL_LOW : CM_GPIO_LEVEL_HIGH);
    return 0;
}

static int bsp_rgb_init(void)
{
    if (bsp_rgb_init_one(APP_RGB_R_GPIO, APP_RGB_R_IOMUX_PIN) != 0) return -1;
    if (bsp_rgb_init_one(APP_RGB_G_GPIO, APP_RGB_G_IOMUX_PIN) != 0) return -1;
    if (bsp_rgb_init_one(APP_RGB_B_GPIO, APP_RGB_B_IOMUX_PIN) != 0) return -1;
    return 0;
}

static void bsp_rgb_set_raw(bool r, bool g, bool b)
{
    cm_gpio_level_e active = APP_RGB_ACTIVE_LEVEL;
    cm_gpio_level_e inactive = (active == CM_GPIO_LEVEL_HIGH) ? CM_GPIO_LEVEL_LOW : CM_GPIO_LEVEL_HIGH;
    cm_gpio_set_level(APP_RGB_R_GPIO, r ? active : inactive);
    cm_gpio_set_level(APP_RGB_G_GPIO, g ? active : inactive);
    cm_gpio_set_level(APP_RGB_B_GPIO, b ? active : inactive);
}

int bsp_rgb_set(bsp_rgb_color_e color)
{
    switch (color) {
        case BSP_RGB_OFF:    bsp_rgb_set_raw(false, false, false); break;
        case BSP_RGB_RED:    bsp_rgb_set_raw(true,  false, false); break;
        case BSP_RGB_GREEN:  bsp_rgb_set_raw(false, true,  false); break;
        case BSP_RGB_BLUE:   bsp_rgb_set_raw(false, false, true);  break;
        case BSP_RGB_YELLOW: bsp_rgb_set_raw(true,  true,  false); break;
        case BSP_RGB_CYAN:   bsp_rgb_set_raw(false, true,  true);  break;
        case BSP_RGB_MAGENTA:bsp_rgb_set_raw(true,  false, true);  break;
        case BSP_RGB_WHITE:  bsp_rgb_set_raw(true,  true,  true);  break;
        default: return -1;
    }
    return 0;
}

void bsp_rgb_blink(bsp_rgb_color_e color, int times, uint32_t on_ms, uint32_t off_ms)
{
    for (int i = 0; i < times; i++) {
        bsp_rgb_set(color);
        osDelay(APP_MS_TO_TICK(on_ms));
        bsp_rgb_set(BSP_RGB_OFF);
        if (i + 1 < times) osDelay(APP_MS_TO_TICK(off_ms));
    }
}

/* 异步 RGB 模式定时器：支持充电慢闪/充满常亮/低电慢闪/平台指令快闪 */
typedef struct {
    osTimerId_t      timer;
    bsp_rgb_pattern_e pattern;
    bsp_rgb_color_e  cur_color;
    int              color_idx;
    bool             on_phase;
    uint32_t         start_tick;
    uint32_t         duration_ms;  /* 0 = 持续 */
    bool             running;
} rgb_pattern_ctx_t;

static rgb_pattern_ctx_t s_rgb_ctx = {0};

/* 平台指令模式：绿→红→蓝循环快闪 */
static const bsp_rgb_color_e s_platform_cmd_colors[] = {
    BSP_RGB_GREEN, BSP_RGB_RED, BSP_RGB_BLUE
};

static void rgb_pattern_apply(bsp_rgb_pattern_e pattern, bool on_phase, int idx)
{
    switch (pattern) {
    case BSP_RGB_PATTERN_CHARGING:
        bsp_rgb_set(on_phase ? BSP_RGB_GREEN : BSP_RGB_OFF);
        break;
    case BSP_RGB_PATTERN_FULL:
        bsp_rgb_set(BSP_RGB_GREEN);
        break;
    case BSP_RGB_PATTERN_LOW_BATTERY:
        bsp_rgb_set(on_phase ? BSP_RGB_RED : BSP_RGB_OFF);
        break;
    case BSP_RGB_PATTERN_PLATFORM_CMD:
        if (on_phase) {
            bsp_rgb_set(s_platform_cmd_colors[idx % 3]);
        } else {
            bsp_rgb_set(BSP_RGB_OFF);
        }
        break;
    default:
        bsp_rgb_set(BSP_RGB_OFF);
        break;
    }
}

static uint32_t rgb_pattern_get_period(bsp_rgb_pattern_e pattern, bool on_phase)
{
    switch (pattern) {
    case BSP_RGB_PATTERN_CHARGING:
    case BSP_RGB_PATTERN_LOW_BATTERY:
        return APP_MS_TO_TICK(on_phase ? APP_RGB_BLINK_SLOW_ON_MS : APP_RGB_BLINK_SLOW_OFF_MS);
    case BSP_RGB_PATTERN_PLATFORM_CMD:
        return APP_MS_TO_TICK(on_phase ? APP_RGB_BLINK_FAST_ON_MS : APP_RGB_BLINK_FAST_OFF_MS);
    case BSP_RGB_PATTERN_FULL:
        return APP_MS_TO_TICK(1000);  /* 常亮，定时器只是用来检查总时长 */
    default:
        return APP_MS_TO_TICK(1000);
    }
}

static void rgb_pattern_cb(void *arg)
{
    (void)arg;
    if (!s_rgb_ctx.running) return;
    /* 检查总时长 */
    if (s_rgb_ctx.duration_ms > 0) {
        uint32_t elapsed = (uint32_t)osKernelGetTickCount() - s_rgb_ctx.start_tick;
        if (elapsed >= APP_MS_TO_TICK(s_rgb_ctx.duration_ms)) {
            bsp_rgb_set(BSP_RGB_OFF);
            s_rgb_ctx.running = false;
            s_rgb_ctx.pattern = BSP_RGB_PATTERN_NONE;
            return;
        }
    }
    /* 切换相位 */
    s_rgb_ctx.on_phase = !s_rgb_ctx.on_phase;
    if (s_rgb_ctx.on_phase && s_rgb_ctx.pattern == BSP_RGB_PATTERN_PLATFORM_CMD) {
        s_rgb_ctx.color_idx++;
    }
    rgb_pattern_apply(s_rgb_ctx.pattern, s_rgb_ctx.on_phase, s_rgb_ctx.color_idx);
    osTimerStart(s_rgb_ctx.timer, rgb_pattern_get_period(s_rgb_ctx.pattern, s_rgb_ctx.on_phase));
}

void bsp_rgb_set_pattern(bsp_rgb_pattern_e pattern, uint32_t duration_sec)
{
    if (s_rgb_ctx.timer && s_rgb_ctx.running) {
        osTimerStop(s_rgb_ctx.timer);
    }
    if (s_rgb_ctx.timer == NULL) {
        osTimerAttr_t tattr = {0};
        tattr.name = "rgb_pat";
        s_rgb_ctx.timer = osTimerNew(rgb_pattern_cb, osTimerOnce, NULL, &tattr);
        if (s_rgb_ctx.timer == NULL) return;
    }
    s_rgb_ctx.pattern     = pattern;
    s_rgb_ctx.color_idx   = 0;
    s_rgb_ctx.on_phase    = true;
    s_rgb_ctx.start_tick  = (uint32_t)osKernelGetTickCount();
    s_rgb_ctx.duration_ms = duration_sec * 1000;
    s_rgb_ctx.running     = (pattern != BSP_RGB_PATTERN_NONE);
    if (s_rgb_ctx.running) {
        rgb_pattern_apply(pattern, true, 0);
        osTimerStart(s_rgb_ctx.timer, rgb_pattern_get_period(pattern, true));
    } else {
        bsp_rgb_set(BSP_RGB_OFF);
    }
}

void bsp_rgb_stop_pattern(void)
{
    if (s_rgb_ctx.timer && s_rgb_ctx.running) {
        osTimerStop(s_rgb_ctx.timer);
    }
    s_rgb_ctx.running = false;
    s_rgb_ctx.pattern = BSP_RGB_PATTERN_NONE;
    bsp_rgb_set(BSP_RGB_OFF);
}

/* ====================================================================
 * 电池电量
 * ==================================================================== */
static int bsp_battery_init(void)
{
    /* ADC 通道一般不需要 iomux（专用），但若占位 IO 用于电压则配置 */
    (void)APP_BATTERY_ADC_IOMUX_PIN;
    return 0;
}

int bsp_battery_read(int *voltage_mv, int *soc)
{
    int32_t raw = 0;
    if (cm_adc_read(APP_BATTERY_ADC_DEV, &raw) != 0) {
        /* 回退到 vbat */
        uint32_t vbat = 0;
        if (cm_adc_vbat_read(&vbat) == 0) {
            raw = (int32_t)vbat;
        } else {
            return -1;
        }
    }
    int mv = raw * APP_BATTERY_DIV_RATIO;
    if (voltage_mv) *voltage_mv = mv;

    if (soc) {
        if (mv >= APP_BATTERY_FULL_MV) {
            *soc = 100;
        } else if (mv <= APP_BATTERY_EMPTY_MV) {
            *soc = 0;
        } else {
            *soc = (mv - APP_BATTERY_EMPTY_MV) * 100 /
                   (APP_BATTERY_FULL_MV - APP_BATTERY_EMPTY_MV);
        }
    }
    return 0;
}

/* ====================================================================
 * 充电状态
 * ==================================================================== */
static int bsp_charge_init(void)
{
    cm_iomux_set_pin_func(APP_CHARGE_STATE_IOMUX_PIN, CM_IOMUX_FUNC_FUNCTION1);
    cm_gpio_cfg_t cfg = {0};
    cfg.direction = CM_GPIO_DIRECTION_INPUT;
    cfg.pull = (APP_CHARGE_ACTIVE_LEVEL == CM_GPIO_LEVEL_HIGH) ? CM_GPIO_PULL_DOWN : CM_GPIO_PULL_UP;
    if (cm_gpio_init(APP_CHARGE_STATE_GPIO, &cfg) != 0) {
        APP_LOGE("charge gpio init fail");
        return -1;
    }
    return 0;
}

bsp_charge_state_e bsp_charging_get_state(int soc)
{
    cm_gpio_level_e level = CM_GPIO_LEVEL_LOW;
    if (cm_gpio_get_level(APP_CHARGE_STATE_GPIO, &level) != 0) {
        return BSP_CHARGE_DISCHARGE;
    }
    if (level != APP_CHARGE_ACTIVE_LEVEL) {
        return BSP_CHARGE_DISCHARGE;
    }
    /* 充电中：插入电源 */
    if (soc >= 100) {
        return BSP_CHARGE_FULL;
    }
    return BSP_CHARGE_CHARGING;
}

/* ====================================================================
 * GPS UART (NMEA 0183)
 * ==================================================================== */
static cm_uart_dev_e   s_gps_dev = APP_GPS_UART_DEV;
static bsp_gps_rx_cb_t s_gps_cb = NULL;
static char            s_gps_line[APP_GPS_RX_BUF_SIZE];
static int             s_gps_line_pos = 0;

static void gps_uart_event_cb(void *param, uint32_t evt)
{
    (void)param;
    if (!(evt & CM_UART_EVENT_TYPE_RX_ARRIVED)) return;

    char buf[64];
    int n;
    while ((n = cm_uart_read(s_gps_dev, buf, sizeof(buf) - 1, 0)) > 0) {
        for (int i = 0; i < n; i++) {
            char c = buf[i];
            if (c == '\r') continue;
            if (c == '\n') {
                if (s_gps_line_pos > 0) {
                    s_gps_line[s_gps_line_pos] = '\0';
                    APP_LOGI("gps rx: %s", s_gps_line);
                    if (s_gps_cb) s_gps_cb(s_gps_line);
                    s_gps_line_pos = 0;
                }
                continue;
            }
            if (s_gps_line_pos < (int)sizeof(s_gps_line) - 1) {
                s_gps_line[s_gps_line_pos++] = c;
            }
        }
    }
}

int bsp_gps_open(bsp_gps_rx_cb_t cb)
{
    cm_iomux_set_pin_func(APP_GPS_UART_PIN_TX, APP_GPS_UART_PIN_TX_FUNC);
    cm_iomux_set_pin_func(APP_GPS_UART_PIN_RX, APP_GPS_UART_PIN_RX_FUNC);

    cm_uart_event_t evt = {0};
    evt.event_type = CM_UART_EVENT_TYPE_RX_ARRIVED | CM_UART_EVENT_TYPE_RX_OVERFLOW;
    evt.event_param = NULL;
    evt.event_entry = (void *)gps_uart_event_cb;
    cm_uart_register_event(s_gps_dev, (void *)&evt);

    cm_uart_cfg_t cfg = {0};
    cfg.byte_size = CM_UART_BYTE_SIZE_8;
    cfg.parity = CM_UART_PARITY_NONE;
    cfg.stop_bit = CM_UART_STOP_BIT_ONE;
    cfg.flow_ctrl = CM_UART_FLOW_CTRL_NONE;
    cfg.baudrate = APP_GPS_UART_BAUDRATE;
    cfg.is_lpuart = 0;
    cfg.rxrb_buf_size = 0;
    if (cm_uart_open(s_gps_dev, &cfg) != 0) {
        APP_LOGE("gps uart open fail");
        return -1;
    }
    s_gps_cb = cb;
    s_gps_line_pos = 0;
    return 0;
}

int bsp_gps_close(void)
{
    return cm_uart_close(s_gps_dev);
}

/* 发送 NMEA 配置指令（自动计算校验和并追加 \r\n）
 * body 不带 $ 和 *cs，如 "CFGLPMODE,2" */
int bsp_gps_send_nmea(const char *body)
{
    if (!body) return -1;
    char buf[80];
    unsigned char cs = 0;
    const char *p;
    for (p = body; *p; p++) cs ^= (unsigned char)*p;
    int n = snprintf(buf, sizeof(buf), "$%s*%02X\r\n", body, cs);
    if (n <= 0 || n >= (int)sizeof(buf)) return -1;
    return cm_uart_write(s_gps_dev, buf, n, 1000);
}

/* 设置 GPS 功耗模式（CFGLPMODE 指令） */
int bsp_gps_set_power_mode(bsp_gps_lpmode_e mode)
{
    char body[24];
    snprintf(body, sizeof(body), "CFGLPMODE,%d", (int)mode);
    return bsp_gps_send_nmea(body);
}

/* 设置 WAKEUP 时长（CFGWT 指令） */
int bsp_gps_set_wakeup_time(int wake_s, int idle_s)
{
    char body[32];
    snprintf(body, sizeof(body), "CFGWT,%d,%d", wake_s, idle_s);
    return bsp_gps_send_nmea(body);
}

/* NMEA 0183 校验：$ 与 * 之间字节按位异或 */
static int nmea_checksum_ok(const char *line)
{
    const char *p = line;
    if (*p != '$') return 0;
    p++;
    unsigned char cs = 0;
    while (*p && *p != '*') {
        cs ^= (unsigned char)*p;
        p++;
    }
    if (*p != '*' || !p[1] || !p[2]) return 0;
    int hi = (p[1] >= 'A') ? (p[1] - 'A' + 10) : (p[1] - '0');
    int lo = (p[2] >= 'A') ? (p[2] - 'A' + 10) : (p[2] - '0');
    return cs == (unsigned char)((hi << 4) | lo);
}

/* 字段分隔：以逗号切分，返回第 idx 个字段（从 0 起） */
static const char *nmea_field(const char *line, int idx)
{
    int i = 0;
    while (i < idx && *line) {
        if (*line == ',') i++;
        line++;
    }
    return line;
}

static double nmea_parse_latlon(const char *field, char dir)
{
    if (!*field) return 0.0;
    double raw = atof(field);
    int deg_int;
    double min_f;
    if (dir == 'N' || dir == 'S') {
        deg_int = (int)(raw / 100);
        min_f = raw - deg_int * 100;
        double v = deg_int + min_f / 60.0;
        if (dir == 'S') v = -v;
        return v;
    } else { /* E / W */
        deg_int = (int)(raw / 100);
        min_f = raw - deg_int * 100;
        double v = deg_int + min_f / 60.0;
        if (dir == 'W') v = -v;
        return v;
    }
}

int bsp_gps_parse_nmea(const char *line, app_location_t *out_loc)
{
    if (!line || !out_loc) return 0;
    if (!nmea_checksum_ok(line)) return 0;

    /* ICOE 协议 GGA 字段：
     * $--GGA,Time,Lat,N,Lon,E,FS,NoSV,HDOP,Msl,M,Altref,M,DiffAge,DiffStation*cs
     *   idx:  0    1    2   3   4   5  6    7    8    9  10  11  12  13      14
     * FS(field6): 0=无效 1=单点定位 2=差分定位（必须 > 0 才算有效定位）
     * NoSV(field7): 参与定位的卫星数量（即需求"卫星连接数"）
     * HDOP(field8): 水平精度因子，accuracy ≈ HDOP × 5m */
    if (strstr(line, "GGA")) {
        const char *fs  = nmea_field(line, 6);
        /* FS=0 表示无定位，丢弃（避免上报无效坐标） */
        if (!*fs || atoi(fs) == 0) return 0;
        const char *lat = nmea_field(line, 2);
        const char *ns  = nmea_field(line, 3);
        const char *lon = nmea_field(line, 4);
        const char *ew  = nmea_field(line, 5);
        const char *sat = nmea_field(line, 7);
        const char *hdop = nmea_field(line, 8);
        const char *alt = nmea_field(line, 9);
        if (*lat && *ns && *lon && *ew) {
            out_loc->latitude  = nmea_parse_latlon(lat, *ns);
            out_loc->longitude = nmea_parse_latlon(lon, *ew);
            out_loc->satellite_cnt = atoi(sat);
            out_loc->altitude = (float)atof(alt);
            /* accuracy 由 HDOP 估算：1σ ≈ HDOP × 5m，2σ ≈ HDOP × 10m
             * 取 2σ 作为 accuracy 更保守 */
            if (*hdop) {
                out_loc->accuracy = (int)(atof(hdop) * 10.0f);
            }
            strcpy(out_loc->source, "GPS");
            strcpy(out_loc->coord_sys, "WGS84");
            return 1;
        }
    } else if (strstr(line, "RMC")) {
        /* ICOE 协议 RMC 字段：
         * $--RMC,Time,Valid,Lat,N,Lon,E,Speed,Course,Date,MagVar,MagVarDir,Mode*cs
         *   idx:  0    1     2   3  4   5  6    7      8     9      10       11
         * Valid(field2): A=有效 V=无效
         * Speed(field7): 速度，单位 knot（1 knot = 1.852 km/h = 0.5144 m/s）
         * Course(field8): 航向，0~359.9 度 */
        const char *status = nmea_field(line, 2);
        if (*status != 'A') return 0; /* V = 无效 */
        const char *lat = nmea_field(line, 3);
        const char *ns  = nmea_field(line, 4);
        const char *lon = nmea_field(line, 5);
        const char *ew  = nmea_field(line, 6);
        const char *spd = nmea_field(line, 7);
        const char *hdg = nmea_field(line, 8);
        if (*lat && *ns && *lon && *ew) {
            out_loc->latitude  = nmea_parse_latlon(lat, *ns);
            out_loc->longitude = nmea_parse_latlon(lon, *ew);
            out_loc->speed   = (float)(atof(spd) * 1.852 / 3.6); /* knot -> m/s */
            out_loc->heading = (float)atof(hdg);
            strcpy(out_loc->source, "GPS");
            strcpy(out_loc->coord_sys, "WGS84");
            return 1;
        }
    } else if (strstr(line, "GSV")) {
        /* ICOE 协议 GSV 字段：
         * $--GSV,NoMsg,MsgNo,NoSv,Sv1,Elv1,Az1,Cno1,...*cs
         *   idx:  0    1     2    3
         * NoSv(field3): 可见卫星总数
         * 注意：GSV 的 NoSv 是"可见卫星数"，GGA 的 NoSV 是"参与定位卫星数"。
         * 需求"卫星连接数"更接近"参与定位的卫星数"，因此 GSV 不更新 satellite_cnt，
         * 仅消费消息不做处理（避免覆盖 GGA 更准确的 NoSV）。 */
        return 0;
    }
    return 0;
}

/* ====================================================================
 * 全局初始化
 * ==================================================================== */
int bsp_init(void)
{
    int ret = 0;
    ret |= bsp_key_init();
    ret |= bsp_buzzer_init();
    ret |= bsp_rgb_init();
    ret |= bsp_battery_init();
    ret |= bsp_charge_init();
    if (ret != 0) {
        APP_LOGW("bsp_init partial fail:%d", ret);
    }
    /* GPS 默认在主任务启动后再 open */
    return ret;
}
