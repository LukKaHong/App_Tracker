/**
 * @file    app_lbs.c
 * @brief   LBS 基站定位兜底模块（OneOsPos 平台）实现
 * @details 将 SDK cm_lbs_* 异步接口封装为同步接口，参照
 *          examples/oneos_lbs/src/oneos_lbs.c。
 *          平台说明：CM_LBS_PLAT_ONEOSPOS 使用模组内置 PID，
 *          与其他用户共享每日 400 万次配额（QPS 30），勿频繁调用。
 */
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "cm_os.h"
#include "cm_modem.h"
#include "cm_lbs.h"
#include "cm_async_dns.h"
#include "app_config.h"
#include "app_log.h"
#include "app_lbs.h"

#if APP_LBS_ENABLE

/* 定位结果缓存（回调上下文 -> 调用任务上下文传递） */
static volatile int  s_lbs_flag = 1;                /* 1=等待中，0=成功，<0=失败错误码 */
static char          s_lbs_longitude[16] = {0};
static char          s_lbs_latitude[16]  = {0};
static char          s_lbs_radius[16]    = {0};
static volatile bool s_lbs_busy = false;            /* 并发调用防护 */

/* LBS 异步回调：SDK HTTP 任务上下文，仅做拷贝+置标志，禁止阻塞操作 */
static void app_lbs_cb(cm_lbs_callback_event_e event,
                       cm_lbs_location_rsp_t *location, void *cb_arg)
{
    (void)cb_arg;
    APP_LOGI("LBS cb fired, event=%d", (int)event);  /* [DIAG] 回调是否被触发 */
    if (event == CM_LBS_LOCATION_OK && location != NULL) {
        if (location->longitude != NULL) {
            strncpy(s_lbs_longitude, location->longitude, sizeof(s_lbs_longitude) - 1);
        }
        if (location->latitude != NULL) {
            strncpy(s_lbs_latitude, location->latitude, sizeof(s_lbs_latitude) - 1);
        }
        if (location->radius != NULL) {
            strncpy(s_lbs_radius, location->radius, sizeof(s_lbs_radius) - 1);
        }
    }
    /* SDK 建议 deinit 在回调中执行（定位结果为异步上送） */
    cm_lbs_deinit();
    s_lbs_flag = event;
}

int app_lbs_get_location(app_location_t *out_loc, int timeout_s)
{
    if (out_loc == NULL || timeout_s <= 0 || timeout_s > 60) {
        return -4;  /* CM_LBS_PARAM_INVALID */
    }

    /* PDP 未激活时 cm_lbs_init 内部 asocket 初始化会永久挂死（实测），
     * 必须先确认 PDP 激活（官方示例 oneos_lbs 同样要求） */
    if (cm_modem_get_pdp_state(1) != 1) {
        APP_LOGE("LBS skip: PDP not active");
        return -1;  /* CM_LBS_NETWORK_ERROR */
    }

    /* 并发防护：同一时刻仅允许一次 LBS 请求 */
    if (s_lbs_busy) {
        return -3;  /* CM_LBS_BUSY */
    }
    s_lbs_busy = true;

    int ret = -2;  /* CM_LBS_UNKNOW_ERR */

    /* 1. 初始化 OneOsPos 定位。
     * [重要] pid 不能传 ""：空 pid 会触发底层 cm_lbs_read_nv_data() 从 MRD NV
     * 读取内置 PID，而该路径的 MRDLock() 以无限等待获取互斥锁，在 ML307C
     * SDK 1.0.0 上开机后 MRD 子系统异常（dm nv read err），导致永久挂死。
     * 修复：直接传入从基线固件反汇编提取的内置默认 PID（0x80261084 处
     * "VuJCIgMPvN"，NV 读取失败时底层兜底也用它），走快速路径绕过 NV。
     * 该 PID 即"模组内置 PID"，与其他模组用户共享每日 400 万次配额（QPS 30）。 */
    cm_lbs_oneospos_attr_t cfg = {
        .pid = "VuJCIgMPvN",
        .time_out = timeout_s,
        .nearbts_enable = 1,
    };
    APP_LOGI("LBS step0: calling cm_lbs_init...");  /* [DIAG] 无此行后续日志=卡在init内部 */
    int init_ret = cm_lbs_init(CM_LBS_PLAT_ONEOSPOS, &cfg);
    APP_LOGI("LBS step0.5: cm_lbs_init returned %d", init_ret);  /* [DIAG] */
    if (init_ret != 0) {
        APP_LOGE("LBS init fail");
        goto out;
    }
    APP_LOGI("LBS init ok (step1)");  /* [DIAG] */

    /* 2. 域名解析 IPv4 优先（底层默认 IPv6 优先，普通 SIM 卡 IPv6 常不可用，
     *     与 MQTT 连接经验一致：IPv6 解析失败导致 ipv4:0 连接错误） */
    cm_async_dns_set_priority(0);

    /* 3. 启动异步定位请求 */
    s_lbs_flag = 1;
    s_lbs_longitude[0] = '\0';
    s_lbs_latitude[0]  = '\0';
    s_lbs_radius[0]    = '\0';
    if (cm_lbs_location(app_lbs_cb, NULL) != 0) {
        cm_lbs_deinit();
        APP_LOGE("LBS request fail");
        goto out;
    }
    APP_LOGI("LBS req sent (step2), waiting cb...");  /* [DIAG] */

    /* 4. 阻塞等待回调结果（SDK 内部 timeout_s + 预留 3s 余量，同官方示例） */
    {
        uint32_t start = (uint32_t)osKernelGetTickCount();
        uint32_t limit = APP_MS_TO_TICK((uint32_t)(timeout_s + 3) * 1000);
        uint32_t next_prog = APP_MS_TO_TICK(5000);
        while (1) {
            if (s_lbs_flag == CM_LBS_LOCATION_OK) {
                ret = 0;
                break;
            }
            if (s_lbs_flag < 0) {
                ret = s_lbs_flag;
                break;
            }
            uint32_t elapsed = (uint32_t)osKernelGetTickCount() - start;
            if (elapsed >= limit) {
                /* 超时未收到回调：自行 deinit 释放 HTTP 通道 */
                cm_lbs_deinit();
                ret = CM_LBS_TIMEOUT;
                break;
            }
            if (elapsed >= next_prog) {  /* [DIAG] 每5秒心跳，证明任务未卡死 */
                APP_LOGI("LBS waiting %us...", elapsed / APP_MS_TO_TICK(1000));
                next_prog += APP_MS_TO_TICK(5000);
            }
            osDelay(APP_MS_TO_TICK(100));
        }
    }

    if (ret != 0) {
        APP_LOGW("LBS fail ret=%d", ret);
        goto out;
    }

    /* 5. 解析结果字符串为数值（先解析到局部变量验证非空，再回填） */
    {
        double lon = strtod(s_lbs_longitude, NULL);
        double lat = strtod(s_lbs_latitude, NULL);
        if (lon == 0.0 && lat == 0.0) {
            APP_LOGW("LBS result empty");
            ret = -2;
            goto out;
        }

        memset(out_loc, 0, sizeof(*out_loc));
        out_loc->longitude = lon;
        out_loc->latitude  = lat;
        out_loc->accuracy  = (int)strtod(s_lbs_radius, NULL);   /* 平台返回精度半径 m，可能为 0 */
        out_loc->satellite_cnt = 0;      /* 基站定位无卫星 */
        strncpy(out_loc->source, "LBS", sizeof(out_loc->source) - 1);
        strncpy(out_loc->coord_sys, APP_LBS_COORD_SYS, sizeof(out_loc->coord_sys) - 1);
        APP_LOGI("LBS ok lon=%.6f lat=%.6f radius=%d",
                 out_loc->longitude, out_loc->latitude, out_loc->accuracy);
    }

out:
    s_lbs_busy = false;
    return ret;
}

#endif /* APP_LBS_ENABLE */
