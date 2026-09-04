/**
 * @file    bsp.c
 * @brief   板级驱动实现（需求文档 V1.12 / 引脚分配 V1.10 定版）
 *          - 蜂鸣器：PWM0 @ Pin74，满占空比输出高电平 = 响，cm_pwm_close = 停
 *          - RUN_LED：PWM1 @ Pin75，闪烁由异步任务控制 PWM 通断，呼吸由占空比渐变
 *          - GPS_PWR_EN：GPIO12 @ Pin22，高电平开
 *          - 充电检测：GPIO3 @ Pin87，高电平 = 充电中
 *          - 电池：ADC0 @ Pin9，4:1 分压
 *          - 计步器：QMA6100P，I2C0 @ Pin57/58，7bit 地址 0x12/0x13（AD0 电平决定）
 *          - GPS：UART1 @ Pin28/29 + NMEA 0183 解析（GGA / RMC / GSV）
 *          LP 睡眠态：GPS UART 与 I2C 引脚配置 SLEEP_FLOAT（pad 级配置一次，
 *          系统进 LP 自动转高阻输入，唤醒自动恢复功能，无需进出 LP 反复操作）。
 *          按键（Pin86）硬件预留，当前版本软件不实现（需求 3 / V1.7）。
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
#include "cm_pwm.h"
#include "cm_i2c.h"
#include "app_log.h"
#include "app_config.h"
#include "bsp.h"

/* PWM 周期（ns）：1ms @ 1KHz，32K 时钟源（低功耗下唯一可用时钟，cm_pwm.h 注意事项）。
 * 满占空比 period_h == period 输出持续高电平（有源蜂鸣器响 / LED 最亮）。 */
#define BSP_PWM_PERIOD_NS       1000000u
#define BSP_PWM_DUTY_FULL       BSP_PWM_PERIOD_NS
#define BSP_PWM_DUTY_HALF       (BSP_PWM_PERIOD_NS / 2u)

/* ====================================================================
 * 蜂鸣器（Pin74 / PWM0，有源高电平触发）
 * ==================================================================== */
static bool s_buzzer_clk_set = false;

static int bsp_buzzer_init(void)
{
    /* PWM 引脚功能：Pin74 FUNCTION1 = PWM0（SDK breathled 示例验证）。
     * 时钟源须在 open 前设置（cm_pwm.h），此处仅预置引脚与时钟，不立即输出。 */
    cm_iomux_set_pin_func(APP_BUZZER_IOMUX_PIN, CM_IOMUX_FUNC_FUNCTION1);
    if (cm_pwm_set_clk(APP_BUZZER_PWM_DEV, CM_PWM_CLK_32K) != 0) {
        APP_LOGE("buzzer pwm set clk fail");
        return -1;
    }
    s_buzzer_clk_set = true;
    return 0;
}

int bsp_buzzer_on(void)
{
    if (!s_buzzer_clk_set) return -1;
    /* 满占空比 = 持续高电平，有源蜂鸣器响 */
    return (cm_pwm_open_ns(APP_BUZZER_PWM_DEV, BSP_PWM_PERIOD_NS, BSP_PWM_DUTY_FULL) == 0) ? 0 : -1;
}

int bsp_buzzer_off(void)
{
    if (!s_buzzer_clk_set) return 0;
    return (cm_pwm_close(APP_BUZZER_PWM_DEV) == 0) ? 0 : -1;
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

/* 异步持续响铃：独立任务实现，避免在 osTimer 回调中操作 PWM（SDK 上下文约束） */
typedef struct {
    uint32_t    on_ms;
    uint32_t    off_ms;
    uint32_t    duration_ms;   /* 0 = 持续 */
    volatile bool running;
} buzzer_async_ctx_t;

static buzzer_async_ctx_t s_buzz_ctx = {0};
static osThreadId_t       s_buzz_thread = NULL;

static void buzzer_async_task(void *arg)
{
    (void)arg;
    uint32_t duration_ms = s_buzz_ctx.duration_ms;
    uint32_t start_tick  = (uint32_t)osKernelGetTickCount();

    while (s_buzz_ctx.running) {
        bsp_buzzer_on();
        osDelay(APP_MS_TO_TICK(s_buzz_ctx.on_ms));

        if (duration_ms > 0 &&
            ((uint32_t)osKernelGetTickCount() - start_tick) >= APP_MS_TO_TICK(duration_ms)) {
            break;
        }

        bsp_buzzer_off();
        osDelay(APP_MS_TO_TICK(s_buzz_ctx.off_ms));

        if (duration_ms > 0 &&
            ((uint32_t)osKernelGetTickCount() - start_tick) >= APP_MS_TO_TICK(duration_ms)) {
            break;
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

    s_buzz_ctx.on_ms       = APP_BUZZER_BEEP_ON_MS;    /* 需求 4：单次鸣响 200ms */
    s_buzz_ctx.off_ms      = APP_BUZZER_BEEP_OFF_MS;   /* 每秒响一次：间隔 800ms */
    s_buzz_ctx.duration_ms = duration_sec * 1000u;
    s_buzz_ctx.running     = true;

    osThreadAttr_t attr = {0};
    attr.name = "buz_async";
    attr.stack_size = 2 * 1024;
    attr.priority = osPriorityBelowNormal1;
    s_buzz_thread = osThreadNew(buzzer_async_task, NULL, &attr);
    if (s_buzz_thread == NULL) {
        s_buzz_ctx.running = false;
        APP_LOGE("buzzer async task create fail");
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
 * RUN_LED（Pin75 / PWM1，单灯）
 * 闪烁 = 任务控制 PWM 通断；呼吸 = 占空比渐变（参照 SDK breathled 示例）
 * ==================================================================== */
static bool s_led_clk_set = false;

static int bsp_led_init(void)
{
    /* Pin75 FUNCTION1 = PWM1（cm_common.h OPENCPU_TEST_PWM1_IOMUX 验证） */
    cm_iomux_set_pin_func(APP_LED_IOMUX_PIN, CM_IOMUX_FUNC_FUNCTION1);
    if (cm_pwm_set_clk(APP_LED_PWM_DEV, CM_PWM_CLK_32K) != 0) {
        APP_LOGE("led pwm set clk fail");
        return -1;
    }
    s_led_clk_set = true;
    return 0;
}

/* 内部：按占空比输出（duty_ns = 0 时关闭 PWM） */
static void led_pwm_output(uint32_t duty_ns)
{
    if (!s_led_clk_set) return;
    if (duty_ns == 0) {
        cm_pwm_close(APP_LED_PWM_DEV);
    } else {
        cm_pwm_open_ns(APP_LED_PWM_DEV, BSP_PWM_PERIOD_NS, duty_ns);
    }
}

int bsp_led_on(void)
{
    if (!s_led_clk_set) return -1;
    led_pwm_output(BSP_PWM_DUTY_FULL);
    return 0;
}

int bsp_led_off(void)
{
    if (!s_led_clk_set) return 0;
    led_pwm_output(0);
    return 0;
}

/* 闪烁相位表：on_ms 亮 / off_ms 灭 */
typedef struct {
    uint16_t on_ms;
    uint16_t off_ms;
} led_phase_t;

/* 快闪 5Hz（需求 5：未联网 / 平台指令） */
static const led_phase_t s_phase_flash[]  = { {100, 100} };
/* 慢闪：每 3 秒闪一次（已联网正常） */
static const led_phase_t s_phase_online[] = { {100, 2900} };
/* 低电量：每秒双闪 */
static const led_phase_t s_phase_lowbat[] = { {100, 150}, {100, 650} };

/* 呼吸参数：20ms 步进，10 步升 + 10 步降，完整周期约 400ms（SDK breathled 同参数） */
#define LED_BREATH_STEP_MS      20
#define LED_BREATH_STEPS        10

typedef struct {
    bsp_led_pattern_e pattern;
    uint32_t    duration_ms;   /* 0 = 持续（常态指示） */
    volatile bool running;
} led_ctx_t;

static led_ctx_t    s_led_ctx = {0};
static osThreadId_t s_led_thread = NULL;

static void led_blink_run(const led_phase_t *phases, int phase_cnt,
                          uint32_t duration_ms, uint32_t start_tick)
{
    int idx = 0;
    while (s_led_ctx.running) {
        led_pwm_output(BSP_PWM_DUTY_FULL);
        osDelay(APP_MS_TO_TICK(phases[idx].on_ms));
        if (duration_ms > 0 &&
            ((uint32_t)osKernelGetTickCount() - start_tick) >= APP_MS_TO_TICK(duration_ms)) {
            break;
        }
        led_pwm_output(0);
        osDelay(APP_MS_TO_TICK(phases[idx].off_ms));
        if (duration_ms > 0 &&
            ((uint32_t)osKernelGetTickCount() - start_tick) >= APP_MS_TO_TICK(duration_ms)) {
            break;
        }
        idx = (idx + 1) % phase_cnt;
    }
}

static void led_breath_run(uint32_t duration_ms, uint32_t start_tick)
{
    int duty = 0;
    bool rising = true;
    while (s_led_ctx.running) {
        led_pwm_output((uint32_t)duty);
        if (duration_ms > 0 &&
            ((uint32_t)osKernelGetTickCount() - start_tick) >= APP_MS_TO_TICK(duration_ms)) {
            break;
        }
        if (rising) {
            duty += (int)(BSP_PWM_PERIOD_NS / LED_BREATH_STEPS);
            if (duty >= (int)BSP_PWM_PERIOD_NS) { duty = (int)BSP_PWM_PERIOD_NS; rising = false; }
        } else {
            duty -= (int)(BSP_PWM_PERIOD_NS / LED_BREATH_STEPS);
            if (duty <= 0) { duty = 0; rising = true; }
        }
        osDelay(APP_MS_TO_TICK(LED_BREATH_STEP_MS));
    }
}

static void led_task(void *arg)
{
    (void)arg;
    bsp_led_pattern_e pattern = s_led_ctx.pattern;
    uint32_t duration_ms = s_led_ctx.duration_ms;
    uint32_t start_tick  = (uint32_t)osKernelGetTickCount();

    switch (pattern) {
        case BSP_LED_PATTERN_ONLINE:
            led_blink_run(s_phase_online, 1, duration_ms, start_tick);
            break;
        case BSP_LED_PATTERN_OFFLINE:
            led_blink_run(s_phase_flash, 1, duration_ms, start_tick);
            break;
        case BSP_LED_PATTERN_LOW_BATTERY:
            led_blink_run(s_phase_lowbat, 2, duration_ms, start_tick);
            break;
        case BSP_LED_PATTERN_BREATH:
            led_breath_run(duration_ms, start_tick);
            break;
        default:
            break;
    }

    led_pwm_output(0);
    s_led_ctx.running = false;
    s_led_thread = NULL;
    /* 任务函数 return 即自动退出 */
}

static void led_task_start(bsp_led_pattern_e pattern, uint32_t duration_ms)
{
    s_led_ctx.pattern     = pattern;
    s_led_ctx.duration_ms = duration_ms;
    s_led_ctx.running     = true;

    osThreadAttr_t attr = {0};
    attr.name = "led_task";
    attr.stack_size = 2 * 1024;
    attr.priority = osPriorityBelowNormal1;
    s_led_thread = osThreadNew(led_task, NULL, &attr);
    if (s_led_thread == NULL) {
        s_led_ctx.running = false;
        led_pwm_output(0);
        APP_LOGE("led task create fail");
    }
}

static void led_task_stop(void)
{
    s_led_ctx.running = false;
    if (s_led_thread) {
        osThreadTerminate(s_led_thread);
        s_led_thread = NULL;
    }
}

void bsp_led_set_pattern(bsp_led_pattern_e pattern)
{
    led_task_stop();
    if (pattern == BSP_LED_PATTERN_OFF) {
        led_pwm_output(0);
        return;
    }
    led_task_start(pattern, 0);   /* 常态指示：持续运行至下次切换 */
}

void bsp_led_flash_async(uint32_t duration_sec)
{
    led_task_stop();
    /* 平台指示灯指令：5Hz 快闪（需求 5），duration_sec = 0 持续至 bsp_led_stop */
    led_task_start(BSP_LED_PATTERN_OFFLINE, duration_sec * 1000u);
}

void bsp_led_stop(void)
{
    led_task_stop();
    led_pwm_output(0);
}

/* ====================================================================
 * GPS_PWR_EN（Pin22 / GPIO12，高电平开）
 * ==================================================================== */
static int bsp_gps_power_init(void)
{
    /* Pin22 主功能 UART0_CTS，FUNCTION1 = GPIO12（资源综述 Table 4） */
    cm_iomux_set_pin_func(APP_GPS_PWR_EN_IOMUX_PIN, APP_GPS_PWR_EN_IOMUX_FUNC);
    cm_gpio_cfg_t cfg = {0};
    cfg.direction = CM_GPIO_DIRECTION_OUTPUT;
    cfg.pull = CM_GPIO_PULL_NONE;
    int32_t ret = cm_gpio_init(APP_GPS_PWR_EN_GPIO, &cfg);
    if (ret != 0) {
        cm_gpio_deinit(APP_GPS_PWR_EN_GPIO);
        ret = cm_gpio_init(APP_GPS_PWR_EN_GPIO, &cfg);
    }
    if (ret != 0) {
        APP_LOGE("gps pwr gpio init fail");
        return -1;
    }
    /* 默认关闭 GNSS 电源（上电不定位，由业务流程按需开启） */
    cm_gpio_set_level(APP_GPS_PWR_EN_GPIO, CM_GPIO_LEVEL_LOW);
    return 0;
}

int bsp_gps_power_on(void)
{
    return (cm_gpio_set_level(APP_GPS_PWR_EN_GPIO, CM_GPIO_LEVEL_HIGH) == 0) ? 0 : -1;
}

int bsp_gps_power_off(void)
{
    return (cm_gpio_set_level(APP_GPS_PWR_EN_GPIO, CM_GPIO_LEVEL_LOW) == 0) ? 0 : -1;
}

/* ====================================================================
 * 充电状态检测（CHRG_State Pin87 / GPIO3，高电平 = 充电中）
 * ==================================================================== */
static int bsp_charge_init(void)
{
    /* Pin87 默认即 GPIO3，无复用功能冲突，无需 iomux 切换 */
    cm_gpio_cfg_t cfg = {0};
    cfg.direction = CM_GPIO_DIRECTION_INPUT;
    cfg.pull = CM_GPIO_PULL_DOWN;   /* 非充电时保证稳定低电平 */
    if (cm_gpio_init(APP_CHRG_GPIO, &cfg) != 0) {
        APP_LOGE("chrg gpio init fail");
        return -1;
    }
    return 0;
}

bool bsp_charge_is_charging(void)
{
    cm_gpio_level_e level = CM_GPIO_LEVEL_LOW;
    if (cm_gpio_get_level(APP_CHRG_GPIO, &level) != 0) {
        return false;
    }
    return (level == APP_CHRG_ACTIVE_LEVEL);
}

/* ====================================================================
 * 电池电量（需求 7 定版：外部分压 200k/68k → ADC1 @ Pin96，
 * 不使用模组内部 VBAT 测量）
 * ==================================================================== */
int bsp_battery_read(int *voltage_mv, int *soc)
{
    int32_t raw = 0;
    if (cm_adc_read(APP_BATTERY_ADC_DEV, &raw) != 0) {
        return -1;
    }
    /* 分压还原（四舍五入）：电池电压 = 引脚电压 × (200+68)/68
     * raw ≤ 1066mV，× 268 无符号溢出风险（< 2^31） */
    int mv = (int)((raw * (APP_BATTERY_DIV_UP_KOHM + APP_BATTERY_DIV_DOWN_KOHM)
                    + APP_BATTERY_DIV_DOWN_KOHM / 2) / APP_BATTERY_DIV_DOWN_KOHM);
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
 * QMA6100P 内置计步器（需求 8，I2C0 @ Pin57/58）
 * 寄存器定义依据《QMA6100P Preliminary Datasheet》Rev A1：
 *  - CHIP_ID  0x00，期望值 0x90
 *  - STEP_CNT 24bit：0x07 低 / 0x08 中 / 0x0D 高
 *    （寄存器图、0x0D 寄存器定义、STEP_CLR 描述三处均指向 0x0D；
 *     0x0E 为 FIFO_ST。仅 0x07~0x08 一节描述文字误写 0x0e，以 0x0D 为准）
 *  - STEP_CONF0 0x12 bit7 = STEP_EN（默认 0x14）
 *  - STEP_CONF1 0x13 bit7 = STEP_CLR（默认 0x7F）
 *  - SR 0x36 写 0xB6 软复位，之后须写回 0x00
 *  - MODE_BIT 0x11 bit7 = 1 进入 Active
 * ==================================================================== */
#define QMA6100P_ADDR_AD0_LOW     0x12   /* AD0 接 GND */
#define QMA6100P_ADDR_AD0_HIGH    0x13   /* AD0 接 VDD */
#define QMA6100P_REG_CHIP_ID      0x00
#define QMA6100P_CHIP_ID_VALUE    0x90
#define QMA6100P_REG_STEP_CNT_L   0x07
#define QMA6100P_REG_STEP_CNT_M   0x08
#define QMA6100P_REG_STEP_CNT_H   0x0D
#define QMA6100P_REG_BW           0x10
#define QMA6100P_REG_MODE         0x11
#define QMA6100P_MODE_ACTIVE      0x80   /* bit7 MODE_BIT */
#define QMA6100P_REG_STEP_CONF0   0x12
#define QMA6100P_STEP_EN          0x80   /* bit7 */
#define QMA6100P_REG_STEP_CONF1   0x13
#define QMA6100P_STEP_CLR         0x80   /* bit7 */
#define QMA6100P_REG_SR           0x36
#define QMA6100P_SOFT_RESET       0xB6

static uint8_t s_qma_addr = 0;   /* 实际探测到的 7bit 地址 */

static int qma_read_reg(uint8_t reg, uint8_t *data, uint32_t len)
{
    /* 标准 I2C 寄存器读：先写寄存器地址，再重复起始读（SDK sc7a20 示例同法） */
    if (cm_i2c_write(APP_GSENSOR_I2C_DEV, s_qma_addr, &reg, 1) < 0) {
        return -1;
    }
    if (cm_i2c_read(APP_GSENSOR_I2C_DEV, s_qma_addr, data, len) != (int32_t)len) {
        return -1;
    }
    return 0;
}

static int qma_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    if (cm_i2c_write(APP_GSENSOR_I2C_DEV, s_qma_addr, buf, 2) != 2) {
        return -1;
    }
    return 0;
}

/* 探测芯片地址：AD0 接法未知（硬件设计），依次尝试 0x12 / 0x13 */
static bool qma_probe(void)
{
    uint8_t id = 0;
    const uint8_t addrs[] = { QMA6100P_ADDR_AD0_LOW, QMA6100P_ADDR_AD0_HIGH };
    for (int i = 0; i < 2; i++) {
        s_qma_addr = addrs[i];
        if (qma_read_reg(QMA6100P_REG_CHIP_ID, &id, 1) == 0 && id == QMA6100P_CHIP_ID_VALUE) {
            APP_LOGI("QMA6100P found at 0x%02X", s_qma_addr);
            return true;
        }
    }
    APP_LOGE("QMA6100P probe fail");
    return false;
}

int bsp_pedometer_init(void)
{
#if !APP_PEDOMETER_ENABLE
    return 0;
#else
    cm_i2c_cfg_t cfg = {
        .addr_type = CM_I2C_ADDR_TYPE_7BIT,
        .mode      = CM_I2C_MODE_MASTER,
        .clk       = CM_I2C_CLK_100KHZ,
    };
    if (cm_i2c_open(APP_GSENSOR_I2C_DEV, &cfg) != 0) {
        APP_LOGE("gsensor i2c open fail");
        return -1;
    }

    /* LP 高阻浮空睡眠态（需求 V1.5：Gsensor 常供电持续计步，IIC 引脚禁止拉低）。
     * pad 级配置一次即可：系统进 LP 自动转高阻输入，唤醒自动恢复 I2C 功能。 */
    cm_iomux_set_pin_cmd(APP_GSENSOR_I2C_SCL_PIN, CM_IOMUX_PINCMD0_SLEEP, CM_IOMUX_PINCMD0_FUNC3_SLEEP_FLOAT);
    cm_iomux_set_pin_cmd(APP_GSENSOR_I2C_SDA_PIN, CM_IOMUX_PINCMD0_SLEEP, CM_IOMUX_PINCMD0_FUNC3_SLEEP_FLOAT);

    if (!qma_probe()) {
        cm_i2c_close(APP_GSENSOR_I2C_DEV);
        return -1;
    }

    /* 软复位恢复默认寄存器（手册：写 0xB6 后须写回 0x00），POR 后 250us 可通讯 */
    if (qma_write_reg(QMA6100P_REG_SR, QMA6100P_SOFT_RESET) != 0 ||
        qma_write_reg(QMA6100P_REG_SR, 0x00) != 0) {
        APP_LOGE("QMA6100P soft reset fail");
        return -1;
    }
    osDelay(APP_MS_TO_TICK(10));   /* 等待复位与 NVM 加载完成 */

    /* 进入 Active 模式（上电默认 Standby） */
    if (qma_write_reg(QMA6100P_REG_MODE, QMA6100P_MODE_ACTIVE) != 0) {
        APP_LOGE("QMA6100P active mode fail");
        return -1;
    }

    /* 使能内置计步器：STEP_EN 置 1，低 7 位 STEP_SAMPLE_CNT 保持默认 */
    uint8_t conf0 = 0;
    if (qma_read_reg(QMA6100P_REG_STEP_CONF0, &conf0, 1) != 0 ||
        qma_write_reg(QMA6100P_REG_STEP_CONF0, conf0 | QMA6100P_STEP_EN) != 0) {
        APP_LOGE("QMA6100P step enable fail");
        return -1;
    }

    APP_LOGI("QMA6100P pedometer init ok");
    return 0;
#endif
}

int bsp_pedometer_read(uint32_t *steps)
{
    if (!steps) return -1;
#if !APP_PEDOMETER_ENABLE
    *steps = 0;
    return 0;
#else
    if (s_qma_addr == 0) return -1;   /* 未初始化 */
    uint8_t lo = 0, mid = 0, hi = 0;
    if (qma_read_reg(QMA6100P_REG_STEP_CNT_L, &lo, 1) != 0 ||
        qma_read_reg(QMA6100P_REG_STEP_CNT_M, &mid, 1) != 0 ||
        qma_read_reg(QMA6100P_REG_STEP_CNT_H, &hi, 1) != 0) {
        return -1;
    }
    *steps = ((uint32_t)hi << 16) | ((uint32_t)mid << 8) | lo;
    return 0;
#endif
}

int bsp_pedometer_clear(void)
{
#if !APP_PEDOMETER_ENABLE
    return 0;
#else
    if (s_qma_addr == 0) return -1;
    uint8_t conf1 = 0;
    if (qma_read_reg(QMA6100P_REG_STEP_CONF1, &conf1, 1) != 0) {
        return -1;
    }
    /* STEP_CLR 置 1 清零 0x0D/0x08/0x07 */
    if (qma_write_reg(QMA6100P_REG_STEP_CONF1, conf1 | QMA6100P_STEP_CLR) != 0) {
        return -1;
    }
    /* 手册未明确 STEP_CLR 是否自清零，保守写回原值 */
    osDelay(APP_MS_TO_TICK(2));
    if (qma_write_reg(QMA6100P_REG_STEP_CONF1, conf1) != 0) {
        return -1;
    }
    return 0;
#endif
}

/* ====================================================================
 * GPS UART (NMEA 0183)
 * ==================================================================== */
static cm_uart_dev_e   s_gps_dev = APP_GPS_UART_DEV;
static bsp_gps_rx_cb_t s_gps_cb = NULL;
static char            s_gps_line[APP_GPS_RX_BUF_SIZE];
static int             s_gps_line_pos = 0;
static bool            s_gps_first_line_logged = false;  /* 会话首行诊断日志 */

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
                    /* 会话首行诊断（每次 GPS 上电后第一条完整行，无论是否合法
                     * NMEA）：区分"芯片无输出"（无此日志）与"波特率不匹配乱码"
                     * （此日志为乱码）与"正常 NMEA 室内无定位"（$G.. 开头） */
                    if (!s_gps_first_line_logged) {
                        s_gps_first_line_logged = true;
                        APP_LOGI("gps: first uart line: %.48s", s_gps_line);
                    }
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

    /* LP 睡眠态（需求 10：LP 模式下 UART 引脚配置为睡眠态，GPS 断电后防止
     * TX 空闲高电平倒灌）。pad 级配置一次，进 LP 自动浮空，唤醒自动恢复。 */
    cm_iomux_set_pin_cmd(APP_GPS_UART_PIN_TX, CM_IOMUX_PINCMD0_SLEEP, CM_IOMUX_PINCMD0_FUNC3_SLEEP_FLOAT);
    cm_iomux_set_pin_cmd(APP_GPS_UART_PIN_RX, CM_IOMUX_PINCMD0_SLEEP, CM_IOMUX_PINCMD0_FUNC3_SLEEP_FLOAT);

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
    s_gps_first_line_logged = false;
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
    int deg_int = (int)(raw / 100);
    double min_f = raw - deg_int * 100;
    double v = deg_int + min_f / 60.0;
    if (dir == 'S' || dir == 'W') v = -v;
    return v;
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
        /* GSV 的 NoSv 是"可见卫星数"，GGA 的 NoSV 是"参与定位卫星数"。
         * 需求"卫星连接数"更接近"参与定位的卫星数"，GSV 不更新 satellite_cnt，
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
    ret |= bsp_buzzer_init();
    ret |= bsp_led_init();
    ret |= bsp_gps_power_init();
    ret |= bsp_charge_init();
    ret |= bsp_pedometer_init();
    if (ret != 0) {
        APP_LOGW("bsp_init partial fail:%d", ret);
    }
    /* GPS UART 由业务流程在 GNSS 上电后调用 bsp_gps_open */
    return ret;
}
