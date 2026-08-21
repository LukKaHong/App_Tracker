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

/* 联调开关（协议 3：HTTP Provisioning 正式流程）
 * 1 = 使用硬编码 MQTT 凭证直连（联调阶段，见 custom_main.c）
 * 0 = 正式流程：缓存凭证优先，无凭证走 SaaS provisioning */
#define APP_USE_HARDCODED_CREDENTIAL 1

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

/* 电池检测功能开关：硬件暂未做电池 SOC 检测电路，置 0 屏蔽。
 * 屏蔽后：不做电量采样、不触发低电/超低电强制 SUPER_SAVE、无低电指示灯，
 * 设备保持联网在线（用于联调测试）。
 * 硬件就绪后改回 1 恢复全部电量管理逻辑。 */
#define APP_BATTERY_ENABLE         0

#define APP_LOW_BATTERY_THRESHOLD   20      /* SOC < 20% : 低电量 */
#define APP_SUPER_LOW_BATTERY        5      /* SOC < 5%  : 超低电量 */

#define APP_KEY_LONGPRESS_MS        5000    /* 长按 5 秒切换开关机 */

/* 需求 1：各模式定位周期 (ms) —— 与协议 mode 字符串一一对应 */
#define APP_INTERVAL_SEARCHING_MS   (10 * 1000)           /* 寻宠 searching：10 秒/次 */
#define APP_INTERVAL_WALKING_MS     (30 * 1000)           /* 遛宠 walking：30 秒/次 */
#define APP_INTERVAL_SUPERVISE_MS   (5  * 60 * 1000)      /* 看护 supervise：5 分/次 */
#define APP_INTERVAL_LOWPOWER_MS    (60 * 60 * 1000)      /* 省电 lowpower：1 小时/次 */
/* 休眠 sleep：不主动上报，收到平台指令后单次定位（app_mode 返回 0） */

/* 需求 1：寻宠/遛宠模式超时自动切回看护模式 */
#define APP_SEARCHING_DURATION_MS   (10 * 60 * 1000)      /* 寻宠 10 分钟后切回看护 */
#define APP_WALKING_DURATION_MS     (30 * 60 * 1000)      /* 遛宠 30 分钟后切回看护 */

/* 指令默认持续时间（HARDWARE_COMMAND_INTEGRATION.md 第 2 节：默认 30 秒） */
#define APP_CMD_DEFAULT_DURATION_S  30

/* ===================================================================
 * 5. GPS 定位芯片 (CC1161W) - UART 通讯
 *    使用 UART_DEV_1 (PIN_28=RX, PIN_29=TX, FUNCTION1)
 * =================================================================== */
#define APP_GPS_UART_DEV            CM_UART_DEV_1
#define APP_GPS_UART_BAUDRATE       CM_UART_BAUDRATE_115200
#define APP_GPS_UART_BAUDRATE_RATE  115200    /* 数值，用于 CFGPRT 命令 */
#define APP_GPS_UART_PIN_RX          CM_IOMUX_PIN_28      /* UART1_RX  */
#define APP_GPS_UART_PIN_TX          CM_IOMUX_PIN_29      /* UART1_TX */
#define APP_GPS_UART_PIN_TX_FUNC    CM_IOMUX_FUNC_FUNCTION1
#define APP_GPS_UART_PIN_RX_FUNC    CM_IOMUX_FUNC_FUNCTION1
#define APP_GPS_RX_BUF_SIZE         (512)
/* [DEBUG] NMEA 原始语句调试：打印 GGA/RMC 原始行。
 * 已完成使命（2026-08-20 排查崩溃根因 + 验证 GGA NoSV 卫星数解析），
 * 正式运行关闭，需要时改回 1 */
#define APP_GPS_NMEA_DEBUG          0

/* ===================================================================
 * 6. 按键 - 中断方式 + 低功耗唤醒
 *    Pin76 / GPIO0（默认 GPIO 第一功能，无需切 FUNCTION）
 *    注：Pin76 功能1为 SMART_BAT，需确认未启用智能电池功能
 * =================================================================== */
#define APP_KEY_GPIO                CM_GPIO_NUM_0
#define APP_KEY_IOMUX_PIN           CM_IOMUX_PIN_76
#define APP_KEY_ACTIVE_LEVEL        CM_GPIO_LEVEL_LOW   /* 低电平表示按下 */
#define APP_KEY_INTERRUPT_MODE      CM_GPIO_IT_EDGE_BOTH

/* ===================================================================
 * 7. 蜂鸣器 - GPIO 驱动（也可改为 PWM）
 *    Pin33 默认 PCM_OUT，复用功能2 = GPIO19
 * =================================================================== */
#define APP_BUZZER_GPIO             CM_GPIO_NUM_19
#define APP_BUZZER_IOMUX_PIN        CM_IOMUX_PIN_33
#define APP_BUZZER_ACTIVE_LEVEL     CM_GPIO_LEVEL_HIGH  /* 高电平响 */
#define APP_BUZZER_BEEP_INTERVAL_MS 1000               /* 1 秒响一次 */

/* ===================================================================
 * 8. RGB 指示灯 - 3 路 GPIO（如使用 PWM 可改为 PWM 设备）
 *    Pin30/32 默认 PCM_CLK/IN，复用功能2 = GPIO16/18
 *    Pin86 默认 GPIO2，功能1 = GPIO2
 * =================================================================== */
#define APP_RGB_R_GPIO              CM_GPIO_NUM_16
#define APP_RGB_R_IOMUX_PIN         CM_IOMUX_PIN_30
#define APP_RGB_R_IOMUX_FUNC        CM_IOMUX_FUNC_FUNCTION2
#define APP_RGB_G_GPIO              CM_GPIO_NUM_2
#define APP_RGB_G_IOMUX_PIN         CM_IOMUX_PIN_86
#define APP_RGB_G_IOMUX_FUNC        CM_IOMUX_FUNC_FUNCTION1
#define APP_RGB_B_GPIO              CM_GPIO_NUM_18
#define APP_RGB_B_IOMUX_PIN         CM_IOMUX_PIN_32
#define APP_RGB_B_IOMUX_FUNC        CM_IOMUX_FUNC_FUNCTION2
#define APP_RGB_ACTIVE_LEVEL        CM_GPIO_LEVEL_HIGH  /* 共阴 LED，高电平点亮 */

/* ===================================================================
 * 9. 电池电量 ADC
 *    Pin9 / ADC0（默认 ADC 功能，量程 0~1.2V）
 *    分压比 4：4.2V ÷ 4 = 1.05V < 1.2V，留余量
 * =================================================================== */
#define APP_BATTERY_ADC_DEV         CM_ADC_0
#define APP_BATTERY_ADC_IOMUX_PIN   CM_IOMUX_PIN_9
#define APP_BATTERY_DIV_RATIO       4                   /* 4:1 分压，4.2V→1.05V < ADC 1.2V 量程 */
#define APP_BATTERY_FULL_MV         4200                /* 满电电压 mV */
#define APP_BATTERY_EMPTY_MV        3300                /* 关机电压 mV */
#define APP_BATTERY_SAMPLE_MS       (30 * 1000)         /* 30 秒采样一次 */

/* ===================================================================
 * 10. 充电状态检测 IO
 *     当前未实现充电检测功能，硬件确定后补充
 * =================================================================== */
#define APP_RGB_BLINK_FAST_ON_MS    100
#define APP_RGB_BLINK_FAST_OFF_MS   100
#define APP_RGB_BLINK_SLOW_ON_MS    500
#define APP_RGB_BLINK_SLOW_OFF_MS   500
#define APP_BUZZER_BEEP_ON_MS       200
#define APP_BUZZER_BEEP_OFF_MS      800

/* ===================================================================
 * 10.1 LBS & WiFi 原始参数上报（需求 2.2）
 *      设备仅采集基站/WiFi 原始参数上报平台，由平台调高德解算。
 *      WiFi 扫描方案：断 MQTT → 静默 8s → 扫描 ~17s → 重连 MQTT
 *      离线时长约 28s（远优于 CFUN=5 方案的 40~90s）
 * =================================================================== */
/* WiFi 参数采集开关：
 * 1 = 启用 WiFi 扫描（GPS 无效时采集 AP 列表，提高室内定位精度）
 * 0 = 仅采集基站参数（bts/nearbts），报文不含 macs 字段 */
#define APP_LBS_WIFI_ENABLE                 0

#define APP_LBS_WIFI_SCAN_MIN_INTERVAL_MS   (5 * 60 * 1000)  /* WiFi 扫描最小间隔 5 分钟 */
#define APP_LBS_WIFI_SCAN_MAX_COUNT         30               /* 期望上报 AP 数量（高德上限 30） */
#define APP_LBS_WIFI_SCAN_ROUND             2                /* 扫描轮次（平衡耗时与成功率） */
#define APP_LBS_WIFI_SCAN_TIMEOUT_S         20               /* 单次扫描超时（秒） */
#define APP_LBS_WIFI_RRC_IDLE_WAIT_S        8                /* 断 MQTT 后静默等待 RRC 回落 IDLE（秒） */
#define APP_LBS_PDP_WAIT_TIMEOUT_S          30               /* 扫描后等待 PDP 激活超时（秒，保留兼容） */
#define APP_LBS_URL_BUF_SIZE                1024             /* bts/nearbts/macs 字符串缓冲 */
#define APP_LBS_JSON_BUF_SIZE               (APP_LBS_URL_BUF_SIZE + 512)

/* ===================================================================
 * WiFi 扫描方案说明（2026-08-21 实测定论）
 * -------------------------------------------------------------------
 * 结论：保持 MQTT TCP 连接时 RRC 永不 IDLE，WiFi 扫描 0 AP；
 *       断开 MQTT 后静默 8s 再扫描，100% 成功（平均 6 AP）。
 * 方案：断 MQTT → 静默等 RRC IDLE → 扫描 → 重连 MQTT
 * 离线时长：约 28s（8s 静默 + ~17s 扫描 + ~3s 重连）
 * 对比 CFUN=5 方案：40~90s 离线，功耗更高
 * =================================================================== */

/* ===================================================================
 * 11. 日志
 * =================================================================== */
#define APP_LOG_TAG                 "[PET]"
#define APP_LOG_ENABLED             1
/* 日志级别过滤（编译时）：NONE < ERROR < WARN < INFO < DEBUG */
#define APP_LOG_LEVEL_NONE          0
#define APP_LOG_LEVEL_ERROR         1
#define APP_LOG_LEVEL_WARN          2
#define APP_LOG_LEVEL_INFO          3
#define APP_LOG_LEVEL_DEBUG         4
#ifndef APP_LOG_LEVEL
#define APP_LOG_LEVEL               APP_LOG_LEVEL_INFO   /* 默认 INFO */
#endif

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
