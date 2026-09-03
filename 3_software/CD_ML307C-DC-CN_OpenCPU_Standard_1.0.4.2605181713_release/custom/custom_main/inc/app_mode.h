/**
 * @file    app_mode.h
 * @brief   运行模式管理（需求 1：五种工作模式，V1.8 起删除软件关机模式）
 *          寻宠 / 遛宠 / 看护 / 省电 / 休眠
 *          模式字符串与平台协议（DEVICE_CLOUD_PROTOCOL.md 5.2 / HARDWARE_COMMAND_INTEGRATION.md）一致
 *          需求 9：工作模式掉电保存，异常复位后恢复复位前模式；首次上电默认看护模式
 */
#ifndef __APP_MODE_H__
#define __APP_MODE_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_MODE_SEARCHING = 0, /* 寻宠 searching：10 秒/次，10 分钟后自动切回看护 */
    APP_MODE_WALKING,       /* 遛宠 walking：30 秒/次，30 分钟后自动切回看护 */
    APP_MODE_SUPERVISE,     /* 看护 supervise：5 分钟/次（默认模式，首次上电进入） */
    APP_MODE_LOWPOWER,      /* 省电 lowpower：1 小时/次 */
    APP_MODE_SLEEP,         /* 休眠 sleep：保持联网不主动上报，收到指令后单次定位 */
    APP_MODE_NUM
} app_mode_e;

/* 初始化：从 flash 恢复掉电前模式；无有效保存则进入默认模式（看护） */
void app_mode_init(void);

/* 当前模式 */
app_mode_e app_mode_get(void);

/* 切换模式（来自平台指令或本地电量/充电触发）；切换即掉电保存
 * 返回 0=成功，<0=失败 */
int app_mode_set(app_mode_e new_mode);

/* 默认模式（首次上电）：看护模式（需求 1） */
app_mode_e app_mode_default(void);

/* ===== 协议字符串映射（searching/walking/supervise/lowpower/sleep）===== */
const char *app_mode_to_string(app_mode_e mode);
/* 协议字符串转枚举；无效返回 APP_MODE_NUM */
app_mode_e app_mode_from_string(const char *mode_str);

/* ===== 寻宠/遛宠超时自动切回看护（需求 1）===== */
void app_mode_reset_switch_timer(void);
/* 当前模式超时到期则自动切回看护并返回 true */
bool app_mode_auto_switch_expired(void);

/* ===== 平台常规定位频率覆盖（LOCATION_FREQUENCY 指令）===== */
/* 设置平台指定的定位周期（秒）；seconds<=0 表示清除恢复模式默认值 */
void app_mode_set_platform_interval(int seconds);
/* 平台是否设置了覆盖值 */
bool app_mode_has_platform_interval(void);

/* 当前模式下定位周期 (ms)，休眠模式返回 0（不主动上报）；
 * 若平台通过 LOCATION_FREQUENCY 设置了覆盖值则优先返回覆盖值 */
uint32_t app_mode_get_loc_interval_ms(void);

#ifdef __cplusplus
}
#endif
#endif /* __APP_MODE_H__ */
