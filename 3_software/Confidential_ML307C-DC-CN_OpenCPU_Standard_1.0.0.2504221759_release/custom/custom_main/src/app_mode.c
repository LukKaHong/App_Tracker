/**
 * @file    app_mode.c
 * @brief   运行模式状态机实现
 */
#include <string.h>
#include "cm_os.h"
#include "app_mode.h"
#include "app_config.h"
#include "app_log.h"

static app_mode_e s_cur_mode = APP_MODE_OFF;
static osMutexId_t s_mode_mutex = NULL;
static uint32_t    s_find_dog_start_tick = 0; /* ms */

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
    s_find_dog_start_tick = 0;
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
    return APP_MODE_NORMAL;
}

int app_mode_set(app_mode_e new_mode)
{
    if (new_mode <= APP_MODE_OFF || new_mode >= APP_MODE_NUM) {
        return -1;
    }
    app_mode_e old;
    mode_lock();
    old = s_cur_mode;
    s_cur_mode = new_mode;
    if (new_mode == APP_MODE_FIND_DOG) {
        s_find_dog_start_tick = (uint32_t)osKernelGetTickCount();
    }
    mode_unlock();
    if (old != new_mode) {
        APP_LOGI("mode %d -> %d", old, new_mode);
    }
    return 0;
}

void app_mode_reset_find_dog_timer(void)
{
    mode_lock();
    s_find_dog_start_tick = (uint32_t)osKernelGetTickCount();
    mode_unlock();
}

bool app_mode_find_dog_expired(void)
{
    bool expired = false;
    mode_lock();
    if (s_cur_mode == APP_MODE_FIND_DOG) {
        uint32_t now = (uint32_t)osKernelGetTickCount();
        if ((now - s_find_dog_start_tick) >= APP_MS_TO_TICK(APP_FIND_DOG_DURATION_MS)) {
            expired = true;
        }
    }
    mode_unlock();
    return expired;
}

uint32_t app_mode_get_loc_interval_ms(void)
{
    app_mode_e m = app_mode_get();
    switch (m) {
        case APP_MODE_FIND_DOG:    return APP_INTERVAL_FIND_DOG_MS;
        case APP_MODE_NORMAL:      return APP_INTERVAL_NORMAL_MS;
        case APP_MODE_SAVE_POWER:  return APP_INTERVAL_SAVEPOWER_MS;
        case APP_MODE_SUPER_SAVE:  return 0; /* 不主动定位 */
        default:                    return 0;
    }
}
