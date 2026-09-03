/**
 * @file    app_mode.c
 * @brief   运行模式状态机实现（需求 1：五种工作模式，V1.8 起删除软件关机模式）
 *          - 寻宠 searching：10s/次，10 分钟自动切回看护
 *          - 遛宠 walking：30s/次，30 分钟自动切回看护
 *          - 看护 supervise：5min/次（默认模式，首次上电进入）
 *          - 省电 lowpower：1h/次
 *          - 休眠 sleep：保持联网不主动上报，指令触发单次定位
 *          需求 9：工作模式掉电保存，异常复位后恢复复位前模式
 */
#include <string.h>
#include "cm_os.h"
#include "app_mode.h"
#include "app_storage.h"
#include "app_config.h"
#include "app_log.h"

static app_mode_e s_cur_mode = APP_MODE_SUPERVISE;
static osMutexId_t s_mode_mutex = NULL;
static uint32_t    s_switch_start_tick = 0;    /* 模式进入时刻（超时切回计时） */
static int         s_platform_interval_s = -1; /* LOCATION_FREQUENCY 覆盖值（秒），-1 未设置 */

static void mode_lock(void)
{
    if (s_mode_mutex) osMutexAcquire(s_mode_mutex, osWaitForever);
}
static void mode_unlock(void)
{
    if (s_mode_mutex) osMutexRelease(s_mode_mutex);
}

static bool mode_is_valid(app_mode_e m)
{
    return (m >= APP_MODE_SEARCHING && m < APP_MODE_NUM);
}

app_mode_e app_mode_default(void)
{
    /* 需求 1：首次上电默认看护模式 */
    return APP_MODE_SUPERVISE;
}

void app_mode_init(void)
{
    if (s_mode_mutex == NULL) {
        osMutexAttr_t mattr = {0};
        mattr.name = "mode_mtx";
        s_mode_mutex = osMutexNew(&mattr);
    }

    /* 需求 9：从 flash 恢复掉电前模式；无有效保存或数值越界则进入默认模式 */
    int saved = -1;
    app_mode_e m = app_mode_default();
    if (app_storage_load_work_mode(&saved) == 0 && mode_is_valid((app_mode_e)saved)) {
        m = (app_mode_e)saved;
        APP_LOGI("work mode restored from flash: %d", saved);
    } else {
        APP_LOGI("no saved work mode, enter default: %d", (int)m);
    }
    s_cur_mode = m;
    /* 异常复位后寻宠/遛宠超时计时重新起算（简化处理，偏安全侧） */
    s_switch_start_tick = (uint32_t)osKernelGetTickCount();
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

int app_mode_set(app_mode_e new_mode)
{
    if (!mode_is_valid(new_mode)) {
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
        APP_LOGI("mode %d -> %d", (int)old, (int)new_mode);
        /* 需求 9：切换即掉电保存。模式切换不频繁（平台指令/超时回切/低电强制），
         * 每次切换写一次 flash，磨损可接受 */
        if (app_storage_save_work_mode((int)new_mode) != 0) {
            APP_LOGW("save work mode %d fail", (int)new_mode);
        }
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
        default:                 return NULL;  /* 无效模式 */
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
    if (expired) {
        APP_LOGI("searching/walking timeout, auto switch to supervise");
        (void)app_storage_save_work_mode((int)APP_MODE_SUPERVISE);
    }
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
    if (m != APP_MODE_SLEEP && s_platform_interval_s > 0) {
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
