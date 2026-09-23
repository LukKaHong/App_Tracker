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
#define APP_FIRMWARE_VERSION        "1.1.0"     /* APP 语义化版本：主.次.修。
                                                * 测试基线：设备经 USB 烧入"1.1.0+publish修复"，
                                                * 平台保留 V1.1.1(1916) 包做 OTA 目标（1.1.0→1.1.1） */
#define APP_HW_VERSION              "HW_V1.0"   /* 硬件版本：跟随 PCB，引脚变更必须升版 */
#define APP_PROTOCOL_VERSION        "v1"
#define APP_MODEL_CODE              "PET-LOCATOR-DEMO"   /* 平台导入型号（配网校验/OTA fw_title 共用，与平台一致） */

/* ===================================================================
 * 2. Provisioning 配置（2026-09-20 联调参数落定）
 * =================================================================== */
#define APP_SAAS_BASE_URL           "http://119.23.217.155"   /* SaaS 激活接口地址前缀 */
#define APP_PROVISIONING_PATH       "/api/device/v1/provision"
/* 一型一密 secret：64 位 hex 字符串，按协议 1.1 以 64 个 ASCII 字节参与
 * HMAC-SHA256（app_util_hmac_sha256_hex 用 strlen 取 key 长度，不做 hex 解码） */
#define APP_PROVISIONING_SECRET     "c9d08936153edc980b39e26c79b1a702c5cabfb8b0419ff8e968296af557e7ab"

/* 联调开关（协议 3：HTTP Provisioning 正式流程）
 * 1 = 使用硬编码 MQTT 凭证直连（联调阶段，见 custom_main.c）
 * 0 = 正式流程：缓存凭证优先，无凭证走 SaaS provisioning
 * 【2026-09-21】首次激活鉴权联调：置 0 启用真实激活流程
 * （NTP 硬前置 → HMAC 签名 POST /provision → 凭证持久化 → MQTT 首连） */
#define APP_USE_HARDCODED_CREDENTIAL 0

/* 【首次激活联调临时脚手架】1 = 每次开机删除已存凭证，强制重走完整
 * 激活流程（custom_main.c main_task 调 app_storage_clear_credential），
 * 用于反复验证 provisioning 请求/响应校验/nonce 重放/HTTP 丢失恢复；
 * 激活链路验证通过后必须改回 0（否则每次上电都重新激活，浪费平台
 * 调用且开机变慢），勿带量产。 */
#define APP_PROV_DEBUG_CLEAR_CRED   0

/* 【临时联调脚手架】平台 mqtt_host 配置错误（下发 127.0.0.1），平台修正前
 * 在激活响应校验通过后强制覆盖 mqtt_host 为测试 broker 地址。
 * 平台修复后 ENABLE 置 0 即恢复使用平台下发值，勿带量产。 */
#define APP_PROV_HOST_OVERRIDE_ENABLE 1
#define APP_PROV_HOST_OVERRIDE_ADDR   "119.23.217.155"

/* Provisioning 失败重试退避（协议 3：获取凭证失败设备应重试） */
#define APP_PROV_RETRY_BACKOFF_MIN_S  30u   /* 首次退避 30 秒 */
#define APP_PROV_RETRY_BACKOFF_MAX_S  300u  /* 指数退避封顶 5 分钟 */

/* ===================================================================
 * 3. 平台 MQTT Topic
 * =================================================================== */
#define APP_MQTT_TOPIC_TELEMETRY    "v1/devices/me/telemetry"
#define APP_MQTT_TOPIC_RPC_REQ      "v1/devices/me/rpc/request/+"
#define APP_MQTT_TOPIC_RPC_RESP     "v1/devices/me/rpc/response/"
/* 【联调协议 V1 2.1】OTA 相关 topic：每次连接/重连后同会话订阅 QoS1，
 * 禁止为 OTA 单独创建第二个 MQTT 连接 */
#define APP_MQTT_TOPIC_ATTR_SUB     "v1/devices/me/attributes"
#define APP_MQTT_TOPIC_ATTR_RESP    "v1/devices/me/attributes/response/+"
#define APP_MQTT_TOPIC_FW_RESP      "v2/fw/response/+/chunk/+"
#define APP_MQTT_TOPIC_FW_ERROR     "v2/fw/error"
#define APP_MQTT_TOPIC_ATTR_REQ     "v1/devices/me/attributes/request/"   /* +requestId */
#define APP_MQTT_TOPIC_FW_REQ       "v2/fw/request/"                      /* +requestId/chunk/N */

/* 【联调协议 V1 2.2/2.4】OTA 快照请求与分片下载参数 */
#define APP_OTA_SHARED_KEYS         "fw_title,fw_version,fw_checksum,fw_checksum_algorithm,fw_size,fw_url"
#define APP_OTA_SNAPSHOT_TIMEOUT_MS (10 * 1000u)  /* 快照响应超时 */
#define APP_OTA_SNAPSHOT_RETRY_MAX  3u            /* 快照请求最多重试次数 */
#define APP_OTA_CHUNK_SIZE          4096u         /* 协议推荐分片大小（允许 1..65536） */
#define APP_OTA_CHUNK_TIMEOUT_MS    (5 * 1000u)   /* 单块响应超时（协议建议 5s） */
#define APP_OTA_CHUNK_RETRY_MAX     3u            /* 单块最多重试次数（协议建议 3） */
#define APP_OTA_FW_SIZE_MAX         (16u * 1024u * 1024u)   /* V1 固件包上限 16MiB */

/* OTA 排障诊断日志开关（2026-09-23 CHECKSUM_MISMATCH 排查期加入）：
 *   1 = 开启：开机 SHA-256 已知答案自检(kat)、快照元数据留痕(meta)、
 *             每分片首 8 字节留痕(cN head)，用于数据级比对定位问题
 *   0 = 关闭（默认，正式版）：诊断日志不参与编译，零开销
 * 排查 OTA 数据/校验类问题时置 1 重新编译即可 */
#define APP_OTA_DIAG_LOG            0

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

/* 电池检测（需求 7：外部分压 200k/68k → Pin96/ADC1）
 * ★★★ 当前=0 仅限无电池的 USB 调试阶段：未接电池时 Pin96 悬空读到假超低电，
 * 60s 后设备被强制切休眠模式，无法调试其余功能。
 * ★★★ 量产烧录前必须恢复为 1（=0 时低电/超低电保护全部失效） */
#define APP_BATTERY_ENABLE          0

#define APP_SUPER_LOW_BATTERY        5      /* SOC < 5%  : 超低电量，上报后强制休眠模式 */

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

/* LP 唤醒调度保护（2026-09-03 实测教训：早于闹钟的外部唤醒差 1s 未到期回睡，
 * 1s 后的 RTC 闹钟因慢时钟域同步延迟未触发，设备睡死到下一个外部事件；
 * 2026-09-07 复发变体：RTC 睡眠精度偏差 ±10~15s（300s 闹钟 ~290s 触发），
 * 提前唤醒回睡后 11s 近期闹钟仍未触发 → 3s 下限保护不足，睡死 4.5h） */
#define APP_LP_DUE_GRACE_S          10   /* 唤醒时距定位到期 ≤10s 直接视为到期执行 */
#define APP_LP_STAY_AWAKE_S         60   /* 距到期 <60s 不入睡：保持清醒空转由 grace 衔接，彻底避免近期闹钟 */
#define APP_LP_ALARM_MIN_S          30   /* RTC 闹钟最小间隔（防御兜底，闹钟晚触发仅延迟不会失败） */

/* ===================================================================
 * 5. GPS 定位芯片 (CC1161W) - UART 通讯
 *    UART1：Pin28=RX, Pin29=TX（GNSS 版本该引脚仅支持 UART1，见资源综述备注）
 *    波特率 115200：CC1161W 默认波特率由 PIO10/LNA_EN strap 电阻决定
 *    （硬件已接 100K 下拉 → 默认 115200），与主控串口天然对齐，
 *    无需 CFGPRT 配置（需求 V1.16 定稿，2026-09-04）
 * =================================================================== */
#define APP_GPS_UART_DEV            CM_UART_DEV_1
#define APP_GPS_UART_BAUDRATE       CM_UART_BAUDRATE_115200
#define APP_GPS_UART_PIN_RX         CM_IOMUX_PIN_28
#define APP_GPS_UART_PIN_TX         CM_IOMUX_PIN_29
#define APP_GPS_UART_PIN_TX_FUNC    CM_IOMUX_FUNC_FUNCTION1
#define APP_GPS_UART_PIN_RX_FUNC    CM_IOMUX_FUNC_FUNCTION1
#define APP_GPS_RX_BUF_SIZE         (512)
/* GPS NMEA 原始语句调试打印（0=关，1=在 bsp_gps_poll 收行处逐行打印原始
 * 语句：含 GSV/GSA 等全部语句类型、校验失败行、配置应答、乱码行）
 * [2026-09-08 改回 0]：GNSS 无定位根因已定位（硬件射频链路），关闭以减少
 * DBG 口日志量；下次排查 GNSS 时再置 1 */
#define APP_GPS_NMEA_DEBUG          0

/* GPS_PWR_EN（需求 10 V1.23：Pin76，GNSS 电路电源控制，高电平开/低电平关）
 * Pin76 复用功能1 = GPIO0（2026-09-05 例程实测）；原 Pin22/GPIO12 已弃用 */
#define APP_GPS_PWR_EN_GPIO         CM_GPIO_NUM_0
#define APP_GPS_PWR_EN_IOMUX_PIN    CM_IOMUX_PIN_76
#define APP_GPS_PWR_EN_IOMUX_FUNC   CM_IOMUX_FUNC_FUNCTION1

/* ===================================================================
 * 6. 按键 KEY（需求 3 / 10：Pin86 硬件预留，当前版本软件不实现）
 *    Pin86 默认 GPIO2（复用功能1 = 32K_OUT，本产品不使用）
 *    电源键接 PWR_ON/OFF（Pin7）硬件开关机，软件不注册 powerkey 回调
 * =================================================================== */
#define APP_KEY_ENABLE              0       /* 0 = 按键功能不实现（硬件预留；LP 唤醒能力独立于此宏） */
#define APP_KEY_GPIO                CM_GPIO_NUM_2
#define APP_KEY_IOMUX_PIN           CM_IOMUX_PIN_86
#define APP_KEY_ACTIVE_LEVEL        CM_GPIO_LEVEL_LOW

/* ===================================================================
 * 7. 蜂鸣器（需求 4 / 10：Pin74，无源蜂鸣器，V1.28 改版）
 *    Pin74 主功能 PWM0（SDK breathled 示例验证 FUNCTION1 = PWM0）。
 *    实现方式：PWM 音频方波驱动（50% 占空比）= 响，关闭 PWM = 停。
 *    频率默认 4kHz：32K 时钟源 8 分频整数，无量化误差；若蜂鸣器
 *    型号谐振频率不同（常见 2731Hz / 2kHz），按规格书调整此宏。
 * =================================================================== */
#define APP_BUZZER_PWM_DEV          CM_PWM_DEV_0
#define APP_BUZZER_IOMUX_PIN        CM_IOMUX_PIN_74
#define APP_BUZZER_FREQ_HZ          4000    /* 无源蜂鸣器驱动频率 Hz（V1.28） */
#define APP_BUZZER_BEEP_ON_MS       200     /* 单次鸣响 200ms（需求 4） */
#define APP_BUZZER_BEEP_OFF_MS      800     /* 每秒响一次：间隔 800ms */

/* ===================================================================
 * 8. 运行指示灯 RUN_LED（需求 5 / 10：Pin75，支持闪烁与呼吸灯）
 *    双模式驱动（2026-09-21 改版）：
 *    - 闪烁/常亮/常灭：GPIO 直控——Pin75 功能2 = GPIO28（资源综述 Table 4）。
 *      原经 PWM 通断实现闪烁，每次 cm_pwm_close 触发 CP 底层固件打印
 *      "port0/1not close clk_en_bit and set reset"（A/B 实验证实为 close
 *      触发，见 bsp.c），5Hz 快闪时 5 条/秒刷屏，故闪烁改 GPIO 直控。
 *    - 呼吸：Pin75 功能1 = PWM1 占空比渐变（cm_common.h：OPENCPU_TEST_PWM1_IOMUX）。
 * =================================================================== */
#define APP_LED_PWM_DEV             CM_PWM_DEV_1
#define APP_LED_IOMUX_PIN           CM_IOMUX_PIN_75
#define APP_LED_GPIO_NUM            CM_GPIO_NUM_28            /* Pin75 功能2 = GPIO28（资源综述 Table 4） */
#define APP_LED_GPIO_IOMUX_FUNC     CM_IOMUX_FUNC_FUNCTION2   /* Pin75 功能2 = GPIO28 */
#define APP_LED_BLINK_FAST_HZ       5       /* 未联网/平台指令：快闪 5Hz（需求 5） */
#define APP_LED_BLINK_SLOW_MS       3000    /* 已联网正常：每 3 秒闪一次 */

/* ===================================================================
 * 9. 电池电量 ADC（需求 7 / 引脚表，2026-09-03 定版；分压改版 2026-09-09 需求 V1.27）
 *    方案：外部分压 200kΩ(上臂,接电池正极) + 20kΩ(下臂,接GND) → Pin96/ADC1
 *    分压比 = (200+20)/20 = 11.0，满电 4.2V → Pin96 ≈ 0.382V < 1.2V 量程（余量充足）
 *    不使用模组内部 VBAT 测量（硬件方案定版）
 * =================================================================== */
#define APP_BATTERY_ADC_DEV         CM_ADC_1            /* Pin96（专用 ADC1 引脚） */
#define APP_BATTERY_DIV_UP_KOHM     200                 /* 分压上臂 kΩ（电池正极侧） */
#define APP_BATTERY_DIV_DOWN_KOHM   20                  /* 分压下臂 kΩ（GND 侧，V1.27 改版） */
/* SOC 估算：查表法（V1.23 实测放电曲线 21 档 + 线性插值，表在 bsp.c）；
 * 单调不增锁存与充电边沿重置见 custom_main.c 电量采样段（需求 7） */
#define APP_BATTERY_SAMPLE_MS       (30 * 1000)         /* 30 秒采样一次 */
/* 超低电连续确认次数（每 30s 采样一次，2 次=60s 确认窗口）：
 * 防单次 ADC 误读直接强制休眠（无 VBAT 交叉校验，软件确认替代） */
#define APP_BATTERY_ULTRA_LOW_CONFIRM 2

/* ===================================================================
 * 10. 充电状态检测（需求 7 / 10，V1.10 定版：CHRG_State Pin87，高电平=充电中）
 *     Pin87 默认 GPIO3，无复用功能冲突
 * =================================================================== */
#define APP_CHRG_GPIO               CM_GPIO_NUM_3
#define APP_CHRG_IOMUX_PIN          CM_IOMUX_PIN_87
#define APP_CHRG_ACTIVE_LEVEL       CM_GPIO_LEVEL_HIGH  /* 高电平 = 充电中 */

/* ===================================================================
 * 11. Gsensor QMA6100P（需求 8 / 10 V1.23：INT=Pin19）
 *     IIC0：SCL=Pin57 / SDA=Pin58（专用功能，无 GPIO 复用）
 *     中断脚 INT=Pin19 / GPIO9：Pin19 主功能非 GPIO，复用功能2 = GPIO9
 *     （官方 lowpower 例程同款配置 cm_iomux_set_pin_func(PIN_19, FUNC2)）；
 *     原 Pin76/GPIO0 已让渡给 GPS_PWR_EN
 * =================================================================== */
#define APP_PEDOMETER_ENABLE        1       /* 计步功能总开关（需求 8） */
#define APP_GSENSOR_I2C_DEV         CM_I2C_DEV_0
#define APP_GSENSOR_I2C_SCL_PIN     CM_IOMUX_PIN_57
#define APP_GSENSOR_I2C_SDA_PIN     CM_IOMUX_PIN_58
#define APP_GSENSOR_INT_GPIO        CM_GPIO_NUM_9
#define APP_GSENSOR_INT_IOMUX_PIN   CM_IOMUX_PIN_19
#define APP_GSENSOR_INT_IOMUX_FUNC  CM_IOMUX_FUNC_FUNCTION2

/* 静止省电判定（需求 8 必做功能：计步增量法，默认启用；仅调试场景关闭） */
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
#define APP_LBS_WIFI_CACHE_VALID_S          (60 * 60)        /* macs 缓存复用有效期（秒）。限频期/
                                                                 * 静止期复用缓存 macs 上报（需求 2.2），
                                                                 * 超期后 macs 置空退化为纯基站解算。
                                                                 * 60min：覆盖看护模式静止判定窗口
                                                                 * （5min 周期 × 2 周期 = 10min）并留余量；
                                                                 * 省电模式（1h 周期）长时间静止后不复用
                                                                 * 陈旧 AP 列表（高德对失效 AP 自动忽略，
                                                                 * 但陈旧列表解算成功率低） */
#define APP_LBS_WIFI_SCAN_MAX_COUNT         30               /* 期望上报 AP 数量（高德上限 30） */
#define APP_LBS_WIFI_SCAN_ROUND             2                /* 扫描轮次（平衡耗时与成功率） */
#define APP_LBS_WIFI_SCAN_TIMEOUT_S         20               /* 单次扫描超时（秒） */
#define APP_LBS_WIFI_RRC_IDLE_WAIT_S        30               /* 断 MQTT 后静默等待 RRC 回落 IDLE（秒）。
                                                                 * 实测定稿（同位置同板子）：
                                                                 * 08-21 静默 10s 成功（当时小区 timer≤10s）；
                                                                 * 09-04 该小区 timer 被调长，8s/10s 均连续
                                                                 * 0 AP（RRC 未回落，扫描 17s 空转）；
                                                                 * 09-04 静默 30s 两次均成功（3/2 AP，扫描
                                                                 * 3~4s 正常完成）。RRC inactivity timer 为
                                                                 * 运营商可调参数（典型 5~20s，本小区实测
                                                                 * 10~30s 区间），30s 为带余量的定稿值。
                                                                 * 若个别小区 timer 更长导致 0 AP，现场按
                                                                 * "wifiscan 0 ap" 告警日志识别。 */
#define APP_LBS_PDP_WAIT_TIMEOUT_S          30               /* 扫描后等待 PDP 激活超时（秒，保留兼容） */
#define APP_LBS_URL_BUF_SIZE                1024             /* bts/nearbts/macs 字符串缓冲 */
#define APP_LBS_JSON_BUF_SIZE               (APP_LBS_URL_BUF_SIZE + 512)

/* WiFi 扫描触发策略（需求 V1.12）：
 * - GNSS 连续 N 个定位周期无效才触发（判定进入室内/遮挡环境）
 * - 寻宠/遛宠模式不触发（扫描窗口 28s 会丢失多个上报周期） */
#define APP_LBS_WIFI_TRIGGER_INVALID_CYCLES 2

/* ===================================================================
 * WiFi 扫描方案说明（2026-09-04 实测定稿，需求 V1.12/V1.16）
 * -------------------------------------------------------------------
 * 原理：ML307C 单路 RF，WiFi 与 LTE 时分共用天线。保持 MQTT TCP 连接时
 *       RRC 永不 IDLE（天线被 LTE 占用），扫描 0 AP；断开 MQTT（仅断
 *       应用层 TCP，保持网络注册与 PDP）后静默等待 RRC 回落 IDLE，
 *       天线释放给 WiFi，扫描正常执行。
 * 方案：断 MQTT → 静默 30s 等 RRC IDLE → 扫描（RRC 已回落时 ~4s）→ 重连
 * 离线窗口：约 37s（30s 静默 + ~4s 扫描 + ~3s 重连），期间保持网络注册，
 *       设备仍可收寻呼，MQTT 重连后补收平台下行。
 * 静默 30s 依据：RRC inactivity timer 为运营商可调参数，本设备实测小区
 *       2026-08-21 时 ≤10s、2026-09-04 已调长至 10~30s 区间，30s 带余量。
 *       风险：若部署小区 timer 更长（>30s）则扫描 0 AP（有告警日志可识别）。
 * CFUN=5 对照实验（2026-09-04，未采纳）：扫描成功 7 AP、本小区恢复 ~9s，
 *       但掉网期间不可寻呼、注网时长不可控（最差 40~90s），且扫描结果
 *       依赖掉网程度；静默方案在网络侧行为变化时表现更稳定可预测。
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

/* ===================================================================
 * 16. 调试实验（临时脚手架，测完即删，勿带量产）
 * =================================================================== */
/* 【回归验证】rti 泄漏修复后 MQTT 断开/重连 120 轮回归（2026-09-14）
 * 背景：A/B 实验已定案泄漏源为 led_task/buzzer 的 terminate+recreate
 * 模式（SDK rti 线程表项不回收已销毁线程），bsp.c 已改常驻任务。
 * 本验证与定位阶段的关键差异：LED 维护【不冻结】——led_status_poll
 * 正常跟随 MQTT 断连切 OFFLINE/ONLINE，复刻故障场景下每周期 2 次的
 * 高频 pattern 切换，验证常驻化后同样节奏不再累积 rti 表项。
 * 判定：120 轮跑满无 EE LOG "rti thread array overflow" 即修复闭环
 * （原故障固件同场景第 73 轮必崩，7/7 零偏差）。
 * 结果：2026-09-14 23:14 已通过（DONE 120 cycles NO crash）。
 * 0 = 关闭（默认）；1 = 启用 */
#define APP_MQTT_LEAK_TEST          0

#ifdef __cplusplus
}
#endif
#endif /* __APP_CONFIG_H__ */
