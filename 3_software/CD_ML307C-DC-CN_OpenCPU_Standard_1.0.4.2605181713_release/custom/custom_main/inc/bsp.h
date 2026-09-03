/**
 * @file    bsp.h
 * @brief   板级硬件驱动（需求文档 V1.12 / 引脚分配 V1.10 定版）
 *          - 蜂鸣器：Pin74 / PWM0，有源高电平触发（满占空比=响，关PWM=停）
 *          - RUN_LED：Pin75 / PWM1，单灯，支持闪烁与呼吸（需求 5）
 *          - GPS_PWR_EN：Pin22 / GPIO12，GNSS 电源控制，高电平开（需求 10）
 *          - 充电检测：CHRG_State Pin87 / GPIO3，高电平=充电中（需求 7）
 *          - 电池电量：ADC0 / Pin9，4:1 分压（需求 7）
 *          - 计步器：QMA6100P 内置计步，I2C0 SCL=Pin57 / SDA=Pin58（需求 8）
 *          - GPS UART：CC1161W NMEA 0183，UART1 Pin28/29
 *          按键（Pin86）硬件预留，当前版本软件不实现（需求 3）。
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

/* ========== 蜂鸣器（需求 4）========== */
int  bsp_buzzer_on(void);
int  bsp_buzzer_off(void);
/* 阻塞鸣响：times 次，每次 on_ms 亮 / off_ms 停 */
void bsp_buzzer_beep(int times, uint32_t on_ms, uint32_t off_ms);
/* 异步持续响：每秒响一次（200ms 鸣 / 800ms 停，需求 4），
 * 持续 duration_sec 秒后自动停止；duration_sec = 0 表示持续响直到 bsp_buzzer_stop() */
void bsp_buzzer_beep_async(uint32_t duration_sec);
void bsp_buzzer_stop(void);

/* ========== RUN_LED 运行指示灯（需求 5）========== */
typedef enum {
    BSP_LED_PATTERN_OFF = 0,      /* 熄灭（休眠/LP 睡眠） */
    BSP_LED_PATTERN_ONLINE,       /* 已联网正常：慢闪（每 3 秒闪一次） */
    BSP_LED_PATTERN_OFFLINE,      /* 未联网：快闪（5Hz） */
    BSP_LED_PATTERN_LOW_BATTERY,  /* 低电量 SOC<20%：每秒双闪 */
    BSP_LED_PATTERN_BREATH,       /* 呼吸灯（硬件能力预留，常态行为未使用） */
} bsp_led_pattern_e;

int  bsp_led_on(void);    /* 常亮 */
int  bsp_led_off(void);   /* 熄灭 */
/* 常态指示模式切换（异步任务实现，持续至下次切换） */
void bsp_led_set_pattern(bsp_led_pattern_e pattern);
/* 平台指示灯指令：快闪 5Hz，持续 duration_sec 秒（需求 5；0 = 持续至 bsp_led_stop） */
void bsp_led_flash_async(uint32_t duration_sec);
/* 停止一切灯效并熄灭（进入 LP 前调用） */
void bsp_led_stop(void);

/* ========== GPS 电源控制（GPS_PWR_EN，高电平开）========== */
int  bsp_gps_power_on(void);
int  bsp_gps_power_off(void);

/* ========== 充电状态检测（高电平 = 充电中）========== */
bool bsp_charge_is_charging(void);

/* ========== 电池电量 ========== */
/* 读取电池电压 (mV) 并换算 SOC (0~100) */
int  bsp_battery_read(int *voltage_mv, int *soc);

/* ========== QMA6100P 内置计步器（需求 8）========== */
/* 初始化：I2C 打开 + CHIP_ID 校验 + 软复位 + Active 模式 + STEP_EN 使能。
 * I2C 引脚同时配置 LP 高阻浮空睡眠态（需求 V1.5：Gsensor 常供电持续计步）。 */
int  bsp_pedometer_init(void);
/* 读取 24bit 计数值（STEP_CNT 0x07/0x08/0x0D）；返回 0=成功 */
int  bsp_pedometer_read(uint32_t *steps);
/* 计数值清零（STEP_CLR 0x13 bit7，用于每天 0 点清零） */
int  bsp_pedometer_clear(void);

/* ========== GPS UART (CC1161W, NMEA 0183) ========== */
/* 初始化 GPS UART，注册接收回调（数据经 bsp_gps_poll 轮询取出后回调） */
typedef void (*bsp_gps_rx_cb_t)(const char *line);
int  bsp_gps_open(bsp_gps_rx_cb_t cb);
int  bsp_gps_close(void);

/* GPS 数据轮询：在 main_task 主循环中以 ~20ms 间隔调用（非阻塞）。
 * [FIX] 替代 UART RX 中断回调——中断上下文不可调用 APP_LOG/osMutex/
 * 复杂解析（SDK 约定），曾导致 OSA tx 重入引发模组静默复位 */
void bsp_gps_poll(void);

/* 设置 GPS 芯片 UART 波特率（CFGPRT 指令），与主控 UART 波特率对齐 */
int  bsp_gps_set_uart_baudrate(uint32_t baud);

/* 解析单行 NMEA 语句，写入 out_loc；非定位语句返回 0 */
int  bsp_gps_parse_nmea(const char *line, app_location_t *out_loc);

/* GPS 功耗模式（对应 ICOE 协议 CFGLPMODE） */
typedef enum {
    BSP_GPS_LPMODE_ULTRA_LOW = 0,  /* 超低功耗（含自适应/系统自动开关/通道控制） */
    BSP_GPS_LPMODE_AUTO     = 1,   /* 自适应功耗模式（软件自动控制） */
    BSP_GPS_LPMODE_HIGH     = 2,   /* 高性能模式（默认） */
    BSP_GPS_LPMODE_FULL     = 3,   /* Full Power Mode (AE Always ON) */
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
