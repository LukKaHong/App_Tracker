/**
 * @file    bsp.h
 * @brief   板级硬件驱动：按键、蜂鸣器、RGB灯、电池、GPS UART
 *          按键长按 5 秒切换开关机；其余外设由应用层调用。
 */
#ifndef __BSP_H__
#define __BSP_H__

#include <stdint.h>
#include <stdbool.h>
#include "app_config.h"
#include "app_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========== 全局初始化 ========== */
int  bsp_init(void);

/* ========== 按键 ========== */
/* 按键事件回调：true 表示长按 5 秒触发，false 表示短按 */
typedef void (*bsp_key_event_cb_t)(bool long_pressed);
void bsp_key_register_cb(bsp_key_event_cb_t cb);
bool bsp_key_is_pressed(void);

/* ========== 蜂鸣器 ========== */
int  bsp_buzzer_on(void);
int  bsp_buzzer_off(void);
void bsp_buzzer_beep(int times, uint32_t on_ms, uint32_t off_ms);
/* 异步持续响：每秒响一次，持续 duration_sec 秒后自动停止；
 * duration_sec = 0 表示持续响直到调用 bsp_buzzer_stop() */
void bsp_buzzer_beep_async(uint32_t duration_sec);
void bsp_buzzer_stop(void);

/* ========== RGB 指示灯 ========== */
typedef enum {
    BSP_RGB_OFF = 0,
    BSP_RGB_RED,
    BSP_RGB_GREEN,
    BSP_RGB_BLUE,
    BSP_RGB_YELLOW,   /* R+G */
    BSP_RGB_CYAN,     /* G+B */
    BSP_RGB_MAGENTA,  /* R+B */
    BSP_RGB_WHITE,    /* R+G+B */
} bsp_rgb_color_e;

int  bsp_rgb_set(bsp_rgb_color_e color);
void bsp_rgb_blink(bsp_rgb_color_e color, int times, uint32_t on_ms, uint32_t off_ms);

/* 异步 RGB 交替快闪：在 colors[] 数组中循环切换（如绿→红→蓝），
 * 持续 duration_sec 秒后自动停止；
 * duration_sec = 0 表示持续闪烁直到调用 bsp_rgb_stop_blink() */
typedef enum {
    BSP_RGB_PATTERN_NONE = 0,
    BSP_RGB_PATTERN_CHARGING,      /* 充电中：绿色慢闪 */
    BSP_RGB_PATTERN_FULL,           /* 充满：绿色常亮 */
    BSP_RGB_PATTERN_LOW_BATTERY,    /* 低电：红色慢闪 */
    BSP_RGB_PATTERN_PLATFORM_CMD,   /* 平台指令：绿→红→蓝快闪 */
} bsp_rgb_pattern_e;

void bsp_rgb_set_pattern(bsp_rgb_pattern_e pattern, uint32_t duration_sec);
void bsp_rgb_stop_pattern(void);

/* ========== 电池电量 ========== */
/* 读取电池电压 (mV) 并换算 SOC (0~100) */
int  bsp_battery_read(int *voltage_mv, int *soc);

/* ========== GPS UART (CC1161W, NMEA 0183) ========== */
/* 初始化 GPS UART，注册接收回调 */
typedef void (*bsp_gps_rx_cb_t)(const char *line);
int  bsp_gps_open(bsp_gps_rx_cb_t cb);
int  bsp_gps_close(void);

/* 设置 GPS 芯片 UART 波特率（CFGPRT 指令），与主控 UART 波特率对齐 */
int  bsp_gps_set_uart_baudrate(uint32_t baud);

/* 解析单行 NMEA 语句，写入 out_loc；非定位语句返回 0 */
int  bsp_gps_parse_nmea(const char *line, app_location_t *out_loc);

/* GPS 功耗模式（对应 ICOE 协议 CFGLPMODE） */
typedef enum {
    BSP_GPS_LPMODE_ULTRA_LOW = 0,  /* 超低功耗（含自适应/系统自动开关/通道控制） */
    BSP_GPS_LPMODE_AUTO     = 1,    /* 自适应功耗模式（软件自动控制） */
    BSP_GPS_LPMODE_HIGH     = 2,    /* 高性能模式（默认） */
    BSP_GPS_LPMODE_FULL     = 3,    /* Full Power Mode (AE Always ON) */
} bsp_gps_lpmode_e;

/* 发送 NMEA 配置指令（自动计算校验和并追加 \r\n）
 * body 不带 $ 和 *cs，如 "CFGLPMODE,2" */
int  bsp_gps_send_nmea(const char *body);

/* 设置 GPS 功耗模式（CFGLPMODE 指令）
 * mode: 0=超低功耗 1=自适应 2=高性能 3=Full Power */
int  bsp_gps_set_power_mode(bsp_gps_lpmode_e mode);

/* 设置 WAKEUP 时长（CFGWT 指令）
 * wake_s: 提前唤醒 AP 时间(秒)
 * idle_s: 进入 IDLE 状态时间(秒) */
int  bsp_gps_set_wakeup_time(int wake_s, int idle_s);


#ifdef __cplusplus
}
#endif
#endif /* __BSP_H__ */
