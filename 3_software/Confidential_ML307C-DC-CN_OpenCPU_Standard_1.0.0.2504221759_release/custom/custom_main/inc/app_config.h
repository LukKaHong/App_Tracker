/**
 * @file    app_config.h
 * @brief   宠物定位器应用层硬件配置
 * @details 所有"待定"的硬件 IO/UART/参数在此集中定义，便于硬件改动后只改一处。
 *          当前为占位默认值，需根据实际原理图修改 TODO 标记项。
 */
#ifndef __APP_CONFIG_H__
#define __APP_CONFIG_H__

#include "cm_gpio.h"
#include "cm_iomux.h"
#include "cm_uart.h"
#include "cm_adc.h"
#include "cm_pwm.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===================================================================
 * 0. 构建版本选择
 *    APP_BUILD_SAMPLE   : 送样版（精简功能，快速送样）
 *    APP_BUILD_STANDARD : 标准版（全功能）
 *    切换版本只需修改此处宏定义
 * =================================================================== */
#define APP_BUILD_SAMPLE           1
#define APP_BUILD_STANDARD         2
#define APP_BUILD_VERSION          APP_BUILD_SAMPLE

/* ===================================================================
 * 1. 固件与设备标识
 * =================================================================== */
#define APP_FIRMWARE_VERSION        "1.0.0"
#define APP_PROTOCOL_VERSION        "v1"
#define APP_MODEL_CODE              "PET-LTE-001"   /* TODO: 与平台导入型号保持一致 */

/* ===================================================================
 * 2. Provisioning 配置（TODO: 联调前替换为真实参数）
 * =================================================================== */
#define APP_SAAS_BASE_URL           "http://saas.example.com"   /* TODO: 替换为真实 SaaS 地址 */
#define APP_PROVISIONING_PATH       "/api/device/v1/provision"
#define APP_PROVISIONING_SECRET     "device-provisioning-test-secret"   /* TODO: 替换为一型一密真实 secret */

/* ===================================================================
 * 3. 平台 MQTT Topic
 * =================================================================== */
#define APP_MQTT_TOPIC_TELEMETRY    "v1/devices/me/telemetry"
#define APP_MQTT_TOPIC_RPC_REQ      "v1/devices/me/rpc/request/+"
#define APP_MQTT_TOPIC_RPC_RESP     "v1/devices/me/rpc/response/"

/* ===================================================================
 * 4. 业务参数
 * =================================================================== */
#define APP_OFFLINE_CACHE_MAX       30      /* 离线缓存最大条数
                                             * 注：每条约 620B，30 条 ≈ 18KB RAM
                                             * 若需 1000 条按需求文档，应改为持久化到
                                             * 文件系统逐条存取（见 app_offline_cache.c TODO） */
#define APP_OFFLINE_REPLAY_RATE     5       /* 离线补传限速：条/秒 */
#define APP_MQTT_KEEPALIVE_SEC      60
#define APP_MSG_ID_MAX_LEN          80

#define APP_LOW_BATTERY_THRESHOLD   20      /* SOC < 20% : 低电量 */
#define APP_SUPER_LOW_BATTERY        5      /* SOC < 5%  : 超低电量 */

#define APP_KEY_LONGPRESS_MS        5000    /* 长按 5 秒切换开关机 */
#define APP_FIND_DOG_DURATION_MS    (10 * 60 * 1000)  /* 寻狗模式 10 分钟 */

/* 各模式定位周期 (ms) */
#define APP_INTERVAL_FIND_DOG_MS    (5  * 1000)
#define APP_INTERVAL_NORMAL_MS      (30 * 1000)
#define APP_INTERVAL_SAVEPOWER_MS   (5  * 60 * 1000)

/* ===================================================================
 * 5. GPS 定位芯片 (CC1161W) - UART 通讯
 *    TODO: 实际 UART 与 PIN 待硬件确定后修改
 * =================================================================== */
#define APP_GPS_UART_DEV            CM_UART_DEV_1
#define APP_GPS_UART_BAUDRATE       CM_UART_BAUDRATE_115200
#define APP_GPS_UART_PIN_TX          CM_IOMUX_PIN_18      /* TODO */
#define APP_GPS_UART_PIN_RX          CM_IOMUX_PIN_19      /* TODO */
#define APP_GPS_UART_PIN_TX_FUNC    CM_IOMUX_FUNC_FUNCTION1   /* TODO */
#define APP_GPS_UART_PIN_RX_FUNC    CM_IOMUX_FUNC_FUNCTION1   /* TODO */
#define APP_GPS_RX_BUF_SIZE         (512)

/* ===================================================================
 * 6. 按键 - 中断方式 + 低功耗唤醒
 *    TODO: 待硬件确定后修改
 * =================================================================== */
#define APP_KEY_GPIO                CM_GPIO_NUM_7       /* TODO */
#define APP_KEY_IOMUX_PIN           CM_IOMUX_PIN_15     /* TODO */
#define APP_KEY_ACTIVE_LEVEL        CM_GPIO_LEVEL_LOW   /* 低电平表示按下（TODO 视电路而定） */
#define APP_KEY_INTERRUPT_MODE      CM_GPIO_IT_EDGE_BOTH

/* ===================================================================
 * 7. 蜂鸣器 - GPIO 驱动（也可改为 PWM）
 *    TODO: 驱动方式与 IO 待硬件确定后修改
 * =================================================================== */
#define APP_BUZZER_GPIO             CM_GPIO_NUM_8       /* TODO */
#define APP_BUZZER_IOMUX_PIN        CM_IOMUX_PIN_16     /* TODO */
#define APP_BUZZER_ACTIVE_LEVEL     CM_GPIO_LEVEL_HIGH  /* 高电平响（TODO 视电路而定） */
#define APP_BUZZER_BEEP_INTERVAL_MS 1000               /* 1 秒响一次 */

/* ===================================================================
 * 8. RGB 指示灯 - 3 路 GPIO（如使用 PWM 可改为 PWM 设备）
 *    TODO: 待硬件确定后修改
 * =================================================================== */
#define APP_RGB_R_GPIO              CM_GPIO_NUM_9       /* TODO */
#define APP_RGB_R_IOMUX_PIN         CM_IOMUX_PIN_17     /* TODO */
#define APP_RGB_G_GPIO              CM_GPIO_NUM_10      /* TODO */
#define APP_RGB_G_IOMUX_PIN         CM_IOMUX_PIN_20     /* TODO */
#define APP_RGB_B_GPIO              CM_GPIO_NUM_11      /* TODO */
#define APP_RGB_B_IOMUX_PIN         CM_IOMUX_PIN_21     /* TODO */
#define APP_RGB_ACTIVE_LEVEL        CM_GPIO_LEVEL_HIGH  /* TODO 视电路而定，共阳/共阴决定 */

/* ===================================================================
 * 9. 电池电量 ADC
 *    TODO: 实际通道与分压比待硬件确定后修改
 * =================================================================== */
#define APP_BATTERY_ADC_DEV         CM_ADC_0            /* TODO */
#define APP_BATTERY_ADC_IOMUX_PIN   CM_IOMUX_PIN_22     /* TODO */
#define APP_BATTERY_DIV_RATIO       2                   /* TODO: ADC 量程 / 电池量程，分压系数 */
#define APP_BATTERY_FULL_MV         4200                /* 满电电压 mV */
#define APP_BATTERY_EMPTY_MV        3300                /* 关机电压 mV */
#define APP_BATTERY_SAMPLE_MS       (30 * 1000)         /* 30 秒采样一次 */

/* ===================================================================
 * 10. 充电状态检测 IO（需求5指示灯必需）
 *     TODO: 待硬件确定后修改
 *     简化方案：用 1 个 GPIO 检测"充电中"（USB 插入），
 *     充满状态由 SOC >= 100 判断
 * =================================================================== */
#define APP_CHARGE_STATE_GPIO       CM_GPIO_NUM_12       /* TODO */
#define APP_CHARGE_STATE_IOMUX_PIN  CM_IOMUX_PIN_23      /* TODO */
#define APP_CHARGE_ACTIVE_LEVEL     CM_GPIO_LEVEL_HIGH   /* TODO: 充电中为高电平 */

#define APP_RGB_BLINK_FAST_ON_MS    100
#define APP_RGB_BLINK_FAST_OFF_MS   100
#define APP_RGB_BLINK_SLOW_ON_MS    500
#define APP_RGB_BLINK_SLOW_OFF_MS   500
#define APP_BUZZER_BEEP_ON_MS       200
#define APP_BUZZER_BEEP_OFF_MS      800

/* ===================================================================
 * 11. 日志
 * =================================================================== */
#define APP_LOG_TAG                 "[PET]"
#define APP_LOG_ENABLED             1

/* ===================================================================
 * 12. 时间换算宏
 *     ML307C SDK OS tick 频率 = 200Hz (1 tick = 5ms)
 *     依据：kernel/common/inc/osa.h  OSA_TICK_FREQ_IN_MILLISEC = 5
 *     osDelay / osTimerStart / osKernelGetTickCount 差值比较均以 tick 为单位，
 *     所有以 ms 表示的延时/超时/比较值必须先用本宏转换为 tick。
 * =================================================================== */
#define APP_TICK_MS                 5u
#define APP_MS_TO_TICK(ms)          ((uint32_t)((ms) / APP_TICK_MS))

#ifdef __cplusplus
}
#endif
#endif /* __APP_CONFIG_H__ */
