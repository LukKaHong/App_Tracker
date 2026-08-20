/**
 * @file    app_mode.c
 * @brief   运行模式状态机实现（需求 1：六种工作模式）
 *          - 寻宠 searching：10s/次，10 分钟自动切回看护
 *          - 遛宠 walking：30s/次，30 分钟自动切回看护
 *          - 看护 supervise：5min/次
 *          - 省电 lowpower：1h/次
 *          - 休眠 sleep：不主动上报，指令触发单次定位
 *          - 关机 off：不联网不定位
 */
#include <string.h>
#include "cm_os.h"
#include "app_mode.h"
#include "app_config.h"
#include "app_log.h"

static app_mode_e s_cur_mode = APP_MODE_OFF;
static osMutexId_t s_mode_mutex = NULL;
static uint32_t    s_switch_start_tick = 0;  /* 模式进入时刻（超时切回计时） */
static int         s_platform_interval_s = -1; /* LOCATION_FREQUENCY 覆盖值（秒），-1 未设置 */

static void mode_lock(void)
{
    if (s_mode_mutex) osMutexAcquire(s_mode_mutex, osWaitForever);
}
static void mode_unlock(void)
{
    if (s_mode_mutex) osMutexRelease(s_mode_mutex);
}

void app_mode_init(void)
{
    if (s_mode_mutex == NULL) {
        osMutexAttr_t mattr = {0};
        mattr.name = "mode_mtx";
        s_mode_mutex = osMutexNew(&mattr);
    }
    s_cur_mode = APP_MODE_OFF;
    s_switch_start_tick = 0;
    s_platform_interval_s = -1;
}

app_mode_e app_mode_get(void)
{
    app_mode_e m;
    mode_lock();
    m = s_cur_mode;
    mode_unlock();
    return m;
}

app_mode_e app_mode_default_on(void)
{
    /* 需求 1：看护模式 - "在关机模式下，长按按键5秒可进入此模式" */
    return APP_MODE_SUPERVISE;
}

int app_mode_set(app_mode_e new_mode)
{
    if (new_mode < APP_MODE_OFF || new_mode >= APP_MODE_NUM) {
        return -1;
    }
    app_mode_e old;
    mode_lock();
    old = s_cur_mode;
    s_cur_mode = new_mode;
    /* 模式实际切换时重置超时计时器（寻宠/遛宠自动切回用） */
    if (old != new_mode) {
        s_switch_start_tick = (uint32_t)osKernelGetTickCount();
    }
    mode_unlock();
    if (old != new_mode) {
        APP_LOGI("mode %d -> %d", old, new_mode);
    }
    return 0;
}

const char *app_mode_to_string(app_mode_e mode)
{
    switch (mode) {
        case APP_MODE_SEARCHING: return "searching";
        case APP_MODE_WALKING:   return "walking";
        case APP_MODE_SUPERVISE: return "supervise";
        case APP_MODE_LOWPOWER:  return "lowpower";
        case APP_MODE_SLEEP:     return "sleep";
        default:                 return NULL;  /* OFF 无协议字符串 */
    }
}

app_mode_e app_mode_from_string(const char *mode_str)
{
    if (!mode_str) return APP_MODE_NUM;
    if (strcmp(mode_str, "searching") == 0) return APP_MODE_SEARCHING;
    if (strcmp(mode_str, "walking") == 0)   return APP_MODE_WALKING;
    if (strcmp(mode_str, "supervise") == 0) return APP_MODE_SUPERVISE;
    if (strcmp(mode_str, "lowpower") == 0)  return APP_MODE_LOWPOWER;
    if (strcmp(mode_str, "sleep") == 0)     return APP_MODE_SLEEP;
    return APP_MODE_NUM;
}

void app_mode_reset_switch_timer(void)
{
    mode_lock();
    s_switch_start_tick = (uint32_t)osKernelGetTickCount();
    mode_unlock();
}

bool app_mode_auto_switch_expired(void)
{
    bool expired = false;
    mode_lock();
    uint32_t duration_ms = 0;
    if (s_cur_mode == APP_MODE_SEARCHING) {
        duration_ms = APP_SEARCHING_DURATION_MS;   /* 需求：寻宠 10 分钟 */
    } else if (s_cur_mode == APP_MODE_WALKING) {
        duration_ms = APP_WALKING_DURATION_MS;     /* 需求：遛宠 30 分钟 */
    }
    if (duration_ms > 0) {
        uint32_t now = (uint32_t)osKernelGetTickCount();
        if ((now - s_switch_start_tick) >= APP_MS_TO_TICK(duration_ms)) {
            /* 自动切回看护模式（需求 1：避免长时间耗电） */
            s_cur_mode = APP_MODE_SUPERVISE;
            s_switch_start_tick = now;
            expired = true;
        }
    }
    mode_unlock();
    return expired;
}

void app_mode_set_platform_interval(int seconds)
{
    mode_lock();
    s_platform_interval_s = (seconds > 0) ? seconds : -1;
    mode_unlock();
    if (seconds > 0) {
        APP_LOGI("platform location interval=%ds", seconds);
    } else {
        APP_LOGI("platform location interval cleared");
    }
}

bool app_mode_has_platform_interval(void)
{
    bool has;
    mode_lock();
    has = (s_platform_interval_s > 0);
    mode_unlock();
    return has;
}

uint32_t app_mode_get_loc_interval_ms(void)
{
    app_mode_e m = app_mode_get();

    /* 平台 LOCATION_FREQUENCY 覆盖值优先（休眠模式仍不主动上报） */
    if (m != APP_MODE_SLEEP && m != APP_MODE_OFF && s_platform_interval_s > 0) {
        return (uint32_t)s_platform_interval_s * 1000u;
    }

    switch (m) {
        case APP_MODE_SEARCHING:  return APP_INTERVAL_SEARCHING_MS;
        case APP_MODE_WALKING:    return APP_INTERVAL_WALKING_MS;
        case APP_MODE_SUPERVISE:  return APP_INTERVAL_SUPERVISE_MS;
        case APP_MODE_LOWPOWER:   return APP_INTERVAL_LOWPOWER_MS;
        case APP_MODE_SLEEP:      return 0;  /* 不主动定位 */
        default:                  return 0;
    }
}
