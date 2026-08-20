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
static uint32_t           s_key_press_tick = 0;
static bool               s_key_pressed = false;
static bool               s_key_longpress_fired = false;

static void key_irq_handler(void)
{
    /* 中断上下文，仅作为 GPIO 中断注册入口；
     * 实际去抖与事件派发由 bsp_key_poll() 周期轮询 GPIO 电平完成 */
}

/* 按键轮询：在 main_task 中以 20ms 间隔调用，替代 osTimer 回调
 * 避免 cm_gpio_get_level 在 System Timer Thread 上下文中触发 OSA 错误 */
void bsp_key_poll(void)
{
    cm_gpio_level_e level = CM_GPIO_LEVEL_LOW;
    cm_gpio_get_level(APP_KEY_GPIO, &level);

    bool pressed_now = (level == APP_KEY_ACTIVE_LEVEL);

    if (pressed_now && !s_key_pressed) {
        /* 按下沿 */
        s_key_pressed = true;
        s_key_press_tick = (uint32_t)osKernelGetTickCount();
        s_key_longpress_fired = false;
    } else if (pressed_now && s_key_pressed) {
        /* 持续按下：达到长按阈值立即触发，无需等待释放 */
        if (!s_key_longpress_fired) {
            uint32_t held = (uint32_t)osKernelGetTickCount() - s_key_press_tick;
            if (held >= APP_MS_TO_TICK(APP_KEY_LONGPRESS_MS)) {
                s_key_longpress_fired = true;
                if (s_key_cb) s_key_cb(true);
            }
        }
    } else if (!pressed_now && s_key_pressed) {
        /* 释放沿 */
        uint32_t held = (uint32_t)osKernelGetTickCount() - s_key_press_tick;
        s_key_pressed = false;
        /* 长按已触发则不再处理短按；仅在未触发长按时判定短按 */
        if (!s_key_longpress_fired) {
            if (held > APP_MS_TO_TICK(50)) {
                if (s_key_cb) s_key_cb(false);
            }
        }
        s_key_longpress_fired = false;
    }
}

static int bsp_key_init(void)
{
    /* Pin76/GPIO0 默认即 GPIO 功能，无需 cm_iomux_set_pin_func
     * 注意：Pin76 的 FUNCTION1 是 SMART_BAT，调用反而会切错 */
    cm_gpio_cfg_t cfg = {0};
    cfg.direction = CM_GPIO_DIRECTION_INPUT;
    cfg.pull = (APP_KEY_ACTIVE_LEVEL == CM_GPIO_LEVEL_LOW) ? CM_GPIO_PULL_UP : CM_GPIO_PULL_DOWN;
    if (cm_gpio_init(APP_KEY_GPIO, &cfg) != 0) {
        APP_LOGE("key gpio init fail");
        return -1;
    }
    /* [DEBUG] 禁用 GPIO 中断以排查 osa_tx_run.c 崩溃：
     * SDK 可能将 GPIO 中断处理延迟到 System Timer Thread 执行，
     * 内部调用 cm_gpio_get_level（已知在此线程不安全）导致 OSA 崩溃。
     * 按键检测已由 main_task 轮询完成，中断仅用于唤醒，暂禁用以验证 */
    /* cm_gpio_interrupt_register(APP_KEY_GPIO, (void *)key_irq_handler); */
    /* cm_gpio_interrupt_enable(APP_KEY_GPIO, APP_KEY_INTERRUPT_MODE); */
    (void)key_irq_handler;
    /* 按键去抖由 main_task 调用 bsp_key_poll() 完成，不使用 osTimer */
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
    int32_t ret;
    /* Pin33 默认功能1=PCM_OUT，功能2=GPIO19，必须切 FUNCTION2 才是 GPIO */
    cm_iomux_set_pin_func(APP_BUZZER_IOMUX_PIN, CM_IOMUX_FUNC_FUNCTION2);
    cm_gpio_cfg_t cfg = {0};
    cfg.direction = CM_GPIO_DIRECTION_OUTPUT;
    cfg.pull = CM_GPIO_PULL_NONE;
    ret = cm_gpio_init(APP_BUZZER_GPIO, &cfg);
    if (ret != 0) {
        cm_gpio_deinit(APP_BUZZER_GPIO);
        ret = cm_gpio_init(APP_BUZZER_GPIO, &cfg);
    }
    if (ret != 0) return -1;
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

/* 异步持续响铃：独立任务实现，避免在 osTimer 回调中调用 cm_gpio_set_level */
typedef struct {
    uint32_t    on_ms;
    uint32_t    off_ms;
    uint32_t    start_tick;
    uint32_t    duration_ms;   /* 0 = 持续 */
    volatile bool running;
} buzzer_async_ctx_t;

static buzzer_async_ctx_t s_buzz_ctx = {0};
static osThreadId_t       s_buzz_thread = NULL;

/* 蜂鸣器异步响铃任务：在独立线程中调用 cm_gpio_set_level */
static void buzzer_async_task(void *arg)
{
    (void)arg;
    uint32_t on_ms      = s_buzz_ctx.on_ms;
    uint32_t off_ms     = s_buzz_ctx.off_ms;
    uint32_t duration_ms = s_buzz_ctx.duration_ms;
    uint32_t start_tick = (uint32_t)osKernelGetTickCount();

    while (s_buzz_ctx.running) {
        /* on 相位 */
        bsp_buzzer_on();
        osDelay(APP_MS_TO_TICK(on_ms));

        /* 检查总时长 */
        if (duration_ms > 0) {
            uint32_t elapsed = (uint32_t)osKernelGetTickCount() - start_tick;
            if (elapsed >= APP_MS_TO_TICK(duration_ms)) break;
        }

        /* off 相位 */
        bsp_buzzer_off();
        osDelay(APP_MS_TO_TICK(off_ms));

        /* 检查总时长 */
        if (duration_ms > 0) {
            uint32_t elapsed = (uint32_t)osKernelGetTickCount() - start_tick;
            if (elapsed >= APP_MS_TO_TICK(duration_ms)) break;
        }
    }

    bsp_buzzer_off();
    s_buzz_ctx.running = false;
    s_buzz_thread = NULL;
    /* 任务函数 return 即自动退出 */
}

void bsp_buzzer_beep_async(uint32_t duration_sec)
{
    /* 停止之前的响铃任务 */
    s_buzz_ctx.running = false;
    if (s_buzz_thread) {
        osThreadTerminate(s_buzz_thread);
        s_buzz_thread = NULL;
    }

    s_buzz_ctx.on_ms      = APP_BUZZER_BEEP_ON_MS;
    s_buzz_ctx.off_ms     = APP_BUZZER_BEEP_OFF_MS;
    s_buzz_ctx.duration_ms = duration_sec * 1000;
    s_buzz_ctx.start_tick = (uint32_t)osKernelGetTickCount();
    s_buzz_ctx.running    = true;

    osThreadAttr_t attr = {0};
    attr.name = "buz_async";
    attr.stack_size = 2 * 1024;
    attr.priority = osPriorityBelowNormal1;   /* SDK 1.0.4 应用层优先级体系 */
    s_buzz_thread = osThreadNew(buzzer_async_task, NULL, &attr);
    if (s_buzz_thread == NULL) {
        s_buzz_ctx.running = false;
    }
}

void bsp_buzzer_stop(void)
{
    s_buzz_ctx.running = false;
    if (s_buzz_thread) {
        osThreadTerminate(s_buzz_thread);
        s_buzz_thread = NULL;
    }
    bsp_buzzer_off();
}

/* ====================================================================
 * RGB LED
 * ==================================================================== */
static int bsp_rgb_init_one(cm_gpio_num_e gpio, cm_iomux_pin_e pin, cm_iomux_func_e func)
{
    int32_t ret;
    cm_iomux_set_pin_func(pin, func);
    cm_gpio_cfg_t cfg = {0};
    cfg.direction = CM_GPIO_DIRECTION_OUTPUT;
    cfg.pull = CM_GPIO_PULL_NONE;
    ret = cm_gpio_init(gpio, &cfg);
    if (ret != 0) {
        cm_gpio_deinit(gpio);
        ret = cm_gpio_init(gpio, &cfg);
    }
    if (ret != 0) return -1;
    cm_gpio_set_level(gpio,
                      (APP_RGB_ACTIVE_LEVEL == CM_GPIO_LEVEL_HIGH) ? CM_GPIO_LEVEL_LOW : CM_GPIO_LEVEL_HIGH);
    return 0;
}

static int bsp_rgb_init(void)
{
    if (bsp_rgb_init_one(APP_RGB_R_GPIO, APP_RGB_R_IOMUX_PIN, APP_RGB_R_IOMUX_FUNC) != 0) return -1;
    if (bsp_rgb_init_one(APP_RGB_G_GPIO, APP_RGB_G_IOMUX_PIN, APP_RGB_G_IOMUX_FUNC) != 0) return -1;
    if (bsp_rgb_init_one(APP_RGB_B_GPIO, APP_RGB_B_IOMUX_PIN, APP_RGB_B_IOMUX_FUNC) != 0) return -1;
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

/* 异步 RGB 模式：独立任务实现，避免在 osTimer 回调中调用 cm_gpio_set_level
 * （SDK 的 GPIO 写 API 在 System Timer Thread 上下文中不安全，会导致 osa_tx_run.c 崩溃） */
typedef struct {
    bsp_rgb_pattern_e pattern;
    bsp_rgb_color_e  cur_color;
    int              color_idx;
    bool             on_phase;
    uint32_t         start_tick;
    uint32_t         duration_ms;  /* 0 = 持续 */
    volatile bool    running;
} rgb_pattern_ctx_t;

static rgb_pattern_ctx_t s_rgb_ctx = {0};
static osThreadId_t      s_rgb_thread = NULL;

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

static uint32_t rgb_pattern_get_on_ms(bsp_rgb_pattern_e pattern)
{
    switch (pattern) {
    case BSP_RGB_PATTERN_CHARGING:
    case BSP_RGB_PATTERN_LOW_BATTERY:
        return APP_RGB_BLINK_SLOW_ON_MS;
    case BSP_RGB_PATTERN_PLATFORM_CMD:
        return APP_RGB_BLINK_FAST_ON_MS;
    case BSP_RGB_PATTERN_FULL:
        return 1000;
    default:
        return 1000;
    }
}

static uint32_t rgb_pattern_get_off_ms(bsp_rgb_pattern_e pattern)
{
    switch (pattern) {
    case BSP_RGB_PATTERN_CHARGING:
    case BSP_RGB_PATTERN_LOW_BATTERY:
        return APP_RGB_BLINK_SLOW_OFF_MS;
    case BSP_RGB_PATTERN_PLATFORM_CMD:
        return APP_RGB_BLINK_FAST_OFF_MS;
    case BSP_RGB_PATTERN_FULL:
        return 0;  /* 常亮，无 off 相位 */
    default:
        return 0;
    }
}

/* RGB 闪烁任务：在独立线程中调用 cm_gpio_set_level，避免 System Timer Thread 崩溃 */
static void rgb_blink_task(void *arg)
{
    (void)arg;
    bsp_rgb_pattern_e pattern = s_rgb_ctx.pattern;
    uint32_t duration_ms = s_rgb_ctx.duration_ms;
    uint32_t start_tick  = (uint32_t)osKernelGetTickCount();
    int color_idx = 0;
    bool on_phase = true;

    /* 初始 on 相位 */
    rgb_pattern_apply(pattern, true, color_idx);

    while (s_rgb_ctx.running) {
        /* 确定当前相位的持续时间 */
        uint32_t phase_ms = on_phase ? rgb_pattern_get_on_ms(pattern)
                                     : rgb_pattern_get_off_ms(pattern);
        if (phase_ms == 0) phase_ms = 1000;  /* 常亮模式 */

        osDelay(APP_MS_TO_TICK(phase_ms));

        /* 检查总时长 */
        if (duration_ms > 0) {
            uint32_t elapsed = (uint32_t)osKernelGetTickCount() - start_tick;
            if (elapsed >= APP_MS_TO_TICK(duration_ms)) break;
        }

        /* 切换相位（常亮模式不切换） */
        if (pattern != BSP_RGB_PATTERN_FULL) {
            on_phase = !on_phase;
            if (on_phase && pattern == BSP_RGB_PATTERN_PLATFORM_CMD) {
                color_idx++;
            }
            rgb_pattern_apply(pattern, on_phase, color_idx);
        }
    }

    bsp_rgb_set(BSP_RGB_OFF);
    s_rgb_ctx.running = false;
    s_rgb_ctx.pattern = BSP_RGB_PATTERN_NONE;
    s_rgb_thread = NULL;
    /* 任务函数 return 即自动退出（CMSIS-RTOS2） */
}

void bsp_rgb_set_pattern(bsp_rgb_pattern_e pattern, uint32_t duration_sec)
{
    /* 停止之前的闪烁任务 */
    s_rgb_ctx.running = false;
    if (s_rgb_thread) {
        osThreadTerminate(s_rgb_thread);
        s_rgb_thread = NULL;
    }

    s_rgb_ctx.pattern     = pattern;
    s_rgb_ctx.color_idx   = 0;
    s_rgb_ctx.on_phase    = true;
    s_rgb_ctx.start_tick  = (uint32_t)osKernelGetTickCount();
    s_rgb_ctx.duration_ms = duration_sec * 1000;
    s_rgb_ctx.running     = (pattern != BSP_RGB_PATTERN_NONE);

    if (s_rgb_ctx.running) {
        osThreadAttr_t attr = {0};
        attr.name = "rgb_blink";
        attr.stack_size = 2 * 1024;
        attr.priority = osPriorityBelowNormal1;   /* SDK 1.0.4 应用层优先级体系 */
        s_rgb_thread = osThreadNew(rgb_blink_task, NULL, &attr);
        if (s_rgb_thread == NULL) {
            s_rgb_ctx.running = false;
            bsp_rgb_set(BSP_RGB_OFF);
        }
    } else {
        bsp_rgb_set(BSP_RGB_OFF);
    }
}

void bsp_rgb_stop_pattern(void)
{
    s_rgb_ctx.running = false;
    if (s_rgb_thread) {
        osThreadTerminate(s_rgb_thread);
        s_rgb_thread = NULL;
    }
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
 * GPS UART (NMEA 0183)
 * ==================================================================== */
static cm_uart_dev_e   s_gps_dev = APP_GPS_UART_DEV;
static bsp_gps_rx_cb_t s_gps_cb = NULL;
static char            s_gps_line[APP_GPS_RX_BUF_SIZE];
static int             s_gps_line_pos = 0;

/* ===== GPS 数据轮询（主循环任务上下文调用，替代 UART 中断回调）=====
 * [FIX] 原 RX_ARRIVED 事件回调在中断上下文执行 NMEA 解析 + APP_LOGI +
 * osMutexAcquire，违反 SDK 约定（cm_uart.h：回调中不可输出 LOG、不可执行
 * 复杂任务）。日志通道与 modem OSA tx 并发重入导致 System Timer Thread
 * osa_tx_run.c:241 崩溃（Tx Status 0x4），模组反复静默复位。
 * 改为主循环 20ms 非阻塞轮询读取，解析/日志/锁均在任务上下文执行。
 * 115200bps 下 20ms 最多约 288 字节，UART 驱动内部接收缓冲可容纳。 */
void bsp_gps_poll(void)
{
    if (!s_gps_cb) return;   /* UART 未打开 */

    char buf[64];
    int n;
    while ((n = cm_uart_read(s_gps_dev, buf, sizeof(buf) - 1, 0)) > 0) {
        for (int i = 0; i < n; i++) {
            char c = buf[i];
            if (c == '\r') continue;
            if (c == '\n') {
                if (s_gps_line_pos > 0) {
                    s_gps_line[s_gps_line_pos] = '\0';
                    s_gps_cb(s_gps_line);
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
    s_gps_cb = NULL;          /* 停止轮询处理 */
    s_gps_line_pos = 0;
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

/* 设置 GPS 芯片 UART 波特率（CFGPRT 指令，ICOE 协议 1.4.2.1）
 * portID=1 (UART0), addr=0, inPro=1 (ICOE), outPro=3 (ICOE+NMEA)
 * 修改后芯片以新波特率输出，主控侧需保持一致 */
int bsp_gps_set_uart_baudrate(uint32_t baud)
{
    char body[40];
    snprintf(body, sizeof(body), "CFGPRT,1,0,%u,1,3", (unsigned)baud);
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
#if APP_GPS_NMEA_DEBUG
        APP_LOGI("nmea GGA: %s", line);
#endif
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
#if APP_GPS_NMEA_DEBUG
        APP_LOGI("nmea RMC: %s", line);
#endif
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
    if (ret != 0) {
        APP_LOGW("bsp_init partial fail:%d", ret);
    }
    /* GPS 默认在主任务启动后再 open */
    return ret;
}
