/**
 * @file    app_config.h
 * @brief   宠物定位器应用层硬件配置（需求文档 V1.12 / 引脚分配 V1.10 定版）
 * @details 所有硬件 IO/UART/参数在此集中定义，硬件改动后只改一处。
 *          引脚功能依据《ML307C_OpenCPU资源综述》Table 4 引脚资源表。
 */
#ifndef __APP_CONFIG_H__
#define __APP_CONFIG_H__

#include "cm_gpio.h"
#include "cm_iomux.h"
#include "cm_uart.h"
#include "cm_adc.h"
#include "cm_pwm.h"
#include "cm_i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===================================================================
 * 1. 固件与设备标识（需求 6.7 软件版本管理）
 * =================================================================== */
#define APP_VERSION_MAJOR           1
#define APP_VERSION_MINOR           1
#define APP_VERSION_PATCH           0
#define APP_FIRMWARE_VERSION        "1.1.0"     /* APP 语义化版本：主.次.修 */
#define APP_HW_VERSION              "HW_V1.0"   /* 硬件版本：跟随 PCB，引脚变更必须升版 */
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
#define APP_OFFLINE_CACHE_MAX       30      /* 离线缓存最大条数（需求 6.2） */
#define APP_OFFLINE_REPLAY_RATE     5       /* 离线补传限速：条/秒 */
#define APP_MSG_ID_MAX_LEN          80

/* MQTT keepalive 按工作模式区分（需求 6.3）：
 * 寻宠/遛宠定位上报即保活（120s）；看护/省电/休眠长间隔（600s） */
#define APP_MQTT_KEEPALIVE_HIGHFREQ_SEC 120
#define APP_MQTT_KEEPALIVE_LP_SEC       600

/* 需求 6.1 离线重连退避：5s → 10s → 30s → 60s 封顶，成功重置 */
#define APP_RECONN_BACKOFF_1_S      5
#define APP_RECONN_BACKOFF_2_S      10
#define APP_RECONN_BACKOFF_3_S      30
#define APP_RECONN_BACKOFF_MAX_S    60
/* 看护/省电模式连续重连失败次数上限：达到后停止重连，等待下一定位周期 */
#define APP_RECONN_MAX_FAIL_LP      5

/* 电池检测（需求 7：硬件已具备 ADC Pin9 电量检测，2026-09-03 确认启用） */
#define APP_BATTERY_ENABLE          1

#define APP_LOW_BATTERY_THRESHOLD   20      /* SOC < 20% : 低电量 */
#define APP_SUPER_LOW_BATTERY        5      /* SOC < 5%  : 超低电量，上报后强制休眠模式 */
#define APP_BATTERY_RECOVER_SOC     10      /* 休眠模式 SOC 回升至此值以上自动恢复看护模式 */

/* ===================================================================
 * 4.1 各模式定位周期（需求 1）
 * =================================================================== */
#define APP_INTERVAL_SEARCHING_MS   (10 * 1000)           /* 寻宠 searching：10 秒/次 */
#define APP_INTERVAL_WALKING_MS     (30 * 1000)           /* 遛宠 walking：30 秒/次 */
#define APP_INTERVAL_SUPERVISE_MS   (5  * 60 * 1000)      /* 看护 supervise：5 分/次 */
#define APP_INTERVAL_LOWPOWER_MS    (60 * 60 * 1000)      /* 省电 lowpower：1 小时/次 */
/* 休眠 sleep：不主动上报，收到平台指令后单次定位（app_mode 返回 0） */

/* 需求 1：寻宠/遛宠模式超时自动切回看护模式 */
#define APP_SEARCHING_DURATION_MS   (10 * 60 * 1000)      /* 寻宠 10 分钟后切回看护 */
#define APP_WALKING_DURATION_MS     (30 * 60 * 1000)      /* 遛宠 30 分钟后切回看护 */

/* 需求 6.3：省电模式定位周期间 15 分钟心跳；休眠模式 30 分钟心跳 */
#define APP_LOWPOWER_HEARTBEAT_MS   (15 * 60 * 1000)
#define APP_SLEEP_HEARTBEAT_MS      (30 * 60 * 1000)

/* 指令默认持续时间（HARDWARE_COMMAND_INTEGRATION.md 第 2 节：默认 30 秒） */
#define APP_CMD_DEFAULT_DURATION_S  30

/* GNSS 单次定位超时（需求 2.1 / LP step5，看护/省电模式，建议值需实测调整） */
#define APP_GNSS_FIX_TIMEOUT_MS     (90 * 1000)

/* ===================================================================
 * 5. GPS 定位芯片 (CC1161W) - UART 通讯
 *    UART1：Pin28=RX, Pin29=TX（GNSS 版本该引脚仅支持 UART1，见资源综述备注）
 * =================================================================== */
#define APP_GPS_UART_DEV            CM_UART_DEV_1
#define APP_GPS_UART_BAUDRATE       CM_UART_BAUDRATE_115200
#define APP_GPS_UART_BAUDRATE_RATE  115200    /* 数值，用于 CFGPRT 命令 */
#define APP_GPS_UART_PIN_RX         CM_IOMUX_PIN_28
#define APP_GPS_UART_PIN_TX         CM_IOMUX_PIN_29
#define APP_GPS_UART_PIN_TX_FUNC    CM_IOMUX_FUNC_FUNCTION1
#define APP_GPS_UART_PIN_RX_FUNC    CM_IOMUX_FUNC_FUNCTION1
#define APP_GPS_RX_BUF_SIZE         (512)
/* [DEBUG] NMEA 原始语句调试：打印 GGA/RMC 原始行。正式运行关闭，需要时改回 1 */
#define APP_GPS_NMEA_DEBUG          0

/* GPS_PWR_EN（需求 10：Pin22，GNSS 电路电源控制，高电平开/低电平关）
 * Pin22 主功能 UART0_CTS，复用功能1 = GPIO12（资源综述 Table 4） */
#define APP_GPS_PWR_EN_GPIO         CM_GPIO_NUM_12
#define APP_GPS_PWR_EN_IOMUX_PIN    CM_IOMUX_PIN_22
#define APP_GPS_PWR_EN_IOMUX_FUNC   CM_IOMUX_FUNC_FUNCTION1

/* ===================================================================
 * 6. 按键 KEY（需求 3 / 10：Pin86 硬件预留，当前版本软件不实现）
 *    Pin86 默认 GPIO2（复用功能1 = 32K_OUT，本产品不使用）
 *    电源键接 PWR_ON/OFF（Pin7）硬件开关机，软件不注册 powerkey 回调
 * =================================================================== */
#define APP_KEY_ENABLE              0       /* 0 = 按键功能不实现（硬件预留） */
#define APP_KEY_GPIO                CM_GPIO_NUM_2
#define APP_KEY_IOMUX_PIN           CM_IOMUX_PIN_86
#define APP_KEY_ACTIVE_LEVEL        CM_GPIO_LEVEL_LOW

/* ===================================================================
 * 7. 蜂鸣器（需求 4 / 10：Pin74，有源蜂鸣器，高电平触发）
 *    Pin74 主功能 PWM0（SDK breathled 示例验证 FUNCTION1 = PWM0）。
 *    实现方式：满占空比输出高电平 = 响，关闭 PWM = 停。
 * =================================================================== */
#define APP_BUZZER_PWM_DEV          CM_PWM_DEV_0
#define APP_BUZZER_IOMUX_PIN        CM_IOMUX_PIN_74
#define APP_BUZZER_BEEP_ON_MS       200     /* 单次鸣响 200ms（需求 4） */
#define APP_BUZZER_BEEP_OFF_MS      800     /* 每秒响一次：间隔 800ms */

/* ===================================================================
 * 8. 运行指示灯 RUN_LED（需求 5 / 10：Pin75，PWM1，支持呼吸灯）
 *    Pin75 主功能 PWM1（cm_common.h：OPENCPU_TEST_PWM1_IOMUX = Pin75/FUNCTION1）。
 *    闪烁通过 PWM 通断实现；呼吸效果通过占空比渐变实现。
 * =================================================================== */
#define APP_LED_PWM_DEV             CM_PWM_DEV_1
#define APP_LED_IOMUX_PIN           CM_IOMUX_PIN_75
#define APP_LED_BLINK_FAST_HZ       5       /* 未联网/平台指令：快闪 5Hz（需求 5） */
#define APP_LED_BLINK_SLOW_MS       3000    /* 已联网正常：每 3 秒闪一次 */
#define APP_LED_LOWBAT_DBL_MS       1000    /* 低电量：每秒双闪 */

/* ===================================================================
 * 9. 电池电量 ADC（需求 7 / 10：Pin9 / ADC0）
 *    分压比 4：4.2V ÷ 4 = 1.05V < 1.2V 量程
 * =================================================================== */
#define APP_BATTERY_ADC_DEV         CM_ADC_0
#define APP_BATTERY_ADC_IOMUX_PIN   CM_IOMUX_PIN_9
#define APP_BATTERY_DIV_RATIO       4                   /* 4:1 分压，4.2V→1.05V < ADC 1.2V 量程 */
#define APP_BATTERY_FULL_MV         4200                /* 满电电压 mV */
#define APP_BATTERY_EMPTY_MV        3300                /* 空电电压 mV（线性近似，放电曲线需实测标定） */
#define APP_BATTERY_SAMPLE_MS       (30 * 1000)         /* 30 秒采样一次 */

/* ===================================================================
 * 10. 充电状态检测（需求 7 / 10，V1.10 定版：CHRG_State Pin87，高电平=充电中）
 *     Pin87 默认 GPIO3，无复用功能冲突
 * =================================================================== */
#define APP_CHRG_GPIO               CM_GPIO_NUM_3
#define APP_CHRG_IOMUX_PIN          CM_IOMUX_PIN_87
#define APP_CHRG_ACTIVE_LEVEL       CM_GPIO_LEVEL_HIGH  /* 高电平 = 充电中 */

/* ===================================================================
 * 11. Gsensor QMA6100P（需求 8 / 10）
 *     IIC0：SCL=Pin57 / SDA=Pin58（专用功能，无 GPIO 复用）
 *     中断脚 INT=Pin76 / GPIO0（默认 GPIO 功能；复用功能1=SMART_BAT，不可启用）
 * =================================================================== */
#define APP_PEDOMETER_ENABLE        1       /* 计步功能总开关（需求 8） */
#define APP_GSENSOR_I2C_DEV         CM_I2C_DEV_0
#define APP_GSENSOR_I2C_SCL_PIN     CM_IOMUX_PIN_57
#define APP_GSENSOR_I2C_SDA_PIN     CM_IOMUX_PIN_58
#define APP_GSENSOR_INT_GPIO        CM_GPIO_NUM_0
#define APP_GSENSOR_INT_IOMUX_PIN   CM_IOMUX_PIN_76

/* 静止省电判定（需求 8，可选优化，平台可配置开关） */
#define APP_STILL_DETECT_ENABLE     1       /* 静止省电总开关 */
#define APP_STILL_STEP_THRESHOLD    3       /* 每定位周期步数增量 ≤ 3 步判定本周期静止 */
#define APP_STILL_CYCLES            2       /* 连续 2 个周期静止判定为静止状态 */

/* ===================================================================
 * 12. LBS & WiFi 原始参数上报（需求 2.2）
 *     设备仅采集基站/WiFi 原始参数上报平台，由平台调高德解算。
 * =================================================================== */
/* WiFi 参数采集开关（需求 V1.12 启用，断 MQTT 不断网方案）：
 * 1 = 启用 WiFi 扫描（GNSS 连续无效时采集 AP 列表，提高室内定位精度）
 * 0 = 仅采集基站参数（bts/nearbts），报文不含 macs 字段 */
#define APP_LBS_WIFI_ENABLE                 1

#define APP_LBS_WIFI_SCAN_MIN_INTERVAL_MS   (5 * 60 * 1000)  /* WiFi 扫描最小间隔 5 分钟 */
#define APP_LBS_WIFI_SCAN_MAX_COUNT         30               /* 期望上报 AP 数量（高德上限 30） */
#define APP_LBS_WIFI_SCAN_ROUND             2                /* 扫描轮次（平衡耗时与成功率） */
#define APP_LBS_WIFI_SCAN_TIMEOUT_S         20               /* 单次扫描超时（秒） */
#define APP_LBS_WIFI_RRC_IDLE_WAIT_S        8                /* 断 MQTT 后静默等待 RRC 回落 IDLE（秒） */
#define APP_LBS_PDP_WAIT_TIMEOUT_S          30               /* 扫描后等待 PDP 激活超时（秒，保留兼容） */
#define APP_LBS_URL_BUF_SIZE                1024             /* bts/nearbts/macs 字符串缓冲 */
#define APP_LBS_JSON_BUF_SIZE               (APP_LBS_URL_BUF_SIZE + 512)

/* WiFi 扫描触发策略（需求 V1.12）：
 * - GNSS 连续 N 个定位周期无效才触发（判定进入室内/遮挡环境）
 * - 寻宠/遛宠模式不触发（扫描窗口 28s 会丢失多个上报周期） */
#define APP_LBS_WIFI_TRIGGER_INVALID_CYCLES 2

/* ===================================================================
 * WiFi 扫描方案说明（2026-08-21 实测定论，需求 V1.12 采纳）
 * -------------------------------------------------------------------
 * 结论：保持 MQTT TCP 连接时 RRC 永不 IDLE，WiFi 扫描 0 AP；
 *       断开 MQTT 后静默 8s 再扫描，100% 成功（平均 6 AP）。
 * 方案：断 MQTT（仅断应用层 TCP，保持网络注册与 PDP）
 *       → 静默等 RRC IDLE → 扫描 → 重连 MQTT
 * 离线时长：约 28s（8s 静默 + ~17s 扫描 + ~3s 重连）
 * CFUN=5 方案弃用：40~90s 离线且需重新注册，功耗更高
 * =================================================================== */

/* ===================================================================
 * 13. 对时机制（需求 6.5：NTP 对时，UTC+8，每天一次）
 * =================================================================== */
#define APP_NTP_ENABLE              1
#define APP_NTP_SERVER              "ntp.aliyun.com"   /* 国内 NTP 服务器 */
#define APP_NTP_TIMEOUT_MS          5000
#define APP_TIMEZONE                8                  /* UTC+8 */
#define APP_NTP_RESYNC_INTERVAL_MS  (24 * 60 * 60 * 1000)  /* 每天对时一次 */

/* ===================================================================
 * 14. 日志
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
 * 15. 时间换算宏
 *     ML307C SDK OS tick 频率 = 200Hz (1 tick = 5ms)
 *     依据：kernel/common/inc/osa.h  OSA_TICK_FREQ_IN_MILLISEC = 5
 * =================================================================== */
#define APP_TICK_MS                 5u
#define APP_MS_TO_TICK(ms)          ((uint32_t)((ms) / APP_TICK_MS))

#ifdef __cplusplus
}
#endif
#endif /* __APP_CONFIG_H__ */
