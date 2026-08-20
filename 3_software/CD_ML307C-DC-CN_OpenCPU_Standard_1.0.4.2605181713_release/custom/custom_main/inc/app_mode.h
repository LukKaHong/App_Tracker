/**
 * @file    app_mode.h
 * @brief   运行模式管理：关机 / 寻狗 / 正常 / 省电 / 超省电
 */
#ifndef __APP_MODE_H__
#define __APP_MODE_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_MODE_OFF = 0,       /* 关机模式 */
    APP_MODE_FIND_DOG,      /* 寻狗模式：5 秒一次定位 */
    APP_MODE_NORMAL,        /* 正常模式：30 秒一次 */
    APP_MODE_SAVE_POWER,    /* 省电模式：5 分钟一次 */
    APP_MODE_SUPER_SAVE,    /* 超省电：只联网不定位 */
    APP_MODE_NUM
} app_mode_e;

/* 初始化 */
void app_mode_init(void);

/* 当前模式 */
app_mode_e app_mode_get(void);

/* 切换模式（来自平台指令或本地按键/电量触发）
 * 返回 0=成功，<0=失败 */
int app_mode_set(app_mode_e new_mode);

/* 默认开机模式（关机模式 -> 开机时进入） */
app_mode_e app_mode_default_on(void);

/* 寻狗模式自动退出倒计时（10 分钟到期回到正常模式） */
void app_mode_reset_find_dog_timer(void);
bool app_mode_find_dog_expired(void);

/* 当前模式下定位周期 (ms)，超省电返回 0 */
uint32_t app_mode_get_loc_interval_ms(void);

#ifdef __cplusplus
}
#endif
#endif /* __APP_MODE_H__ */
