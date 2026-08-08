/**
 * @file    app_lbs.c
 * @brief   LBS 基站定位封装实现（OneOSPOS 平台）
 *          作为 GPS 超时回退，提供粗略位置。
 *          cm_lbs_location 为异步接口，此处封装为同步阻塞等待，
 *          回调中仅拷贝结果+置标志+deinit（遵循项目惯例：回调内不操作堆/不阻塞）。
 *          OneOSPOS 返回 GCJ02 坐标，端侧转为 WGS84 后与 GPS 坐标系统一。
 */
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "cm_os.h"
#include "cm_lbs.h"
#include "app_config.h"
#include "app_log.h"
#include "app_lbs.h"

#if APP_LBS_ENABLED

/* ===================================================================
 * GCJ02(火星坐标) -> WGS84 转换（公开偏移算法）
 * =================================================================== */
#define PI          3.14159265358979324
#define A           6378245.0
#define EE          0.00669342162296594

static double transform_lat(double x, double y)
{
    double ret = -100.0 + 2.0 * x + 3.0 * y + 0.2 * y * y +
                 0.1 * x * y + 0.2 * sqrt(x > 0 ? x : -x);
    ret += (20.0 * sin(6.0 * x * PI) + 20.0 * sin(2.0 * x * PI)) * 2.0 / 3.0;
    ret += (20.0 * sin(y * PI) + 40.0 * sin(y / 3.0 * PI)) * 2.0 / 3.0;
    ret += (160.0 * sin(y / 12.0 * PI) + 320 * sin(y * PI / 30.0)) * 2.0 / 3.0;
    return ret;
}

static double transform_lon(double x, double y)
{
    double ret = 300.0 + x + 2.0 * y + 0.1 * x * x +
                 0.1 * x * y + 0.1 * sqrt(x > 0 ? x : -x);
    ret += (20.0 * sin(6.0 * x * PI) + 20.0 * sin(2.0 * x * PI)) * 2.0 / 3.0;
    ret += (20.0 * sin(x * PI) + 40.0 * sin(x / 3.0 * PI)) * 2.0 / 3.0;
    ret += (150.0 * sin(x / 12.0 * PI) + 300.0 * sin(x / 30.0 * PI)) * 2.0 / 3.0;
    return ret;
}

/* GCJ02 -> WGS84，输出转换后的经纬度 */
static void gcj02_to_wgs84(double gcj_lon, double gcj_lat,
                            double *out_lon, double *out_lat)
{
    double dlat = transform_lat(gcj_lon - 105.0, gcj_lat - 35.0);
    double dlon = transform_lon(gcj_lon - 105.0, gcj_lat - 35.0);
    double radlat = gcj_lat / 180.0 * PI;
    double magic = sin(radlat);
    magic = 1 - EE * magic * magic;
    double sqrtmagic = sqrt(magic);
    dlat = (dlat * 180.0) / ((A * (1 - EE)) / (magic * sqrtmagic) * PI);
    dlon = (dlon * 180.0) / (A / sqrtmagic * cos(radlat) * PI);
    *out_lat = gcj_lat - dlat;
    *out_lon = gcj_lon - dlon;
}

/* ===================================================================
 * 异步回调 -> 同步等待
 * 回调运行在 SDK 内部任务上下文，仅做：拷贝结果 + 置标志 + deinit
 * =================================================================== */
static volatile bool s_lbs_done  = false;
static volatile int  s_lbs_event = 0;
static char s_lon_str[16];
static char s_lat_str[16];
static char s_radius_str[16];

static void lbs_cb(cm_lbs_callback_event_e event,
                    cm_lbs_location_rsp_t *location, void *cb_arg)
{
    (void)cb_arg;
    s_lbs_event = (int)event;

    if (event == CM_LBS_LOCATION_OK && location) {
        if (location->longitude) {
            strncpy(s_lon_str, location->longitude, sizeof(s_lon_str) - 1);
            s_lon_str[sizeof(s_lon_str) - 1] = '\0';
        }
        if (location->latitude) {
            strncpy(s_lat_str, location->latitude, sizeof(s_lat_str) - 1);
            s_lat_str[sizeof(s_lat_str) - 1] = '\0';
        }
        if (location->radius) {
            strncpy(s_radius_str, location->radius, sizeof(s_radius_str) - 1);
            s_radius_str[sizeof(s_radius_str) - 1] = '\0';
        }
    }
    s_lbs_done = true;

    /* SDK 要求在回调中调用 deinit 以释放占用的 1 路 HTTP 资源 */
    cm_lbs_deinit();
}

/* 同步获取一次 LBS 基站定位 */
int app_lbs_get_location(app_location_t *out_loc)
{
    if (!out_loc) return -1;

    /* 重置状态 */
    s_lbs_done = false;
    s_lbs_event = 0;
    s_lon_str[0] = s_lat_str[0] = s_radius_str[0] = '\0';

    /* OneOSPOS 配置 */
    cm_lbs_oneospos_attr_t cfg = {0};
    cfg.pid = (char *)APP_LBS_PID;
    cfg.time_out = APP_LBS_TIMEOUT_MS / 1000;
    cfg.nearbts_enable = APP_LBS_NEARBTS_ENABLE;

    int ret = cm_lbs_init(CM_LBS_PLAT_ONEOSPOS, &cfg);
    if (ret != 0) {
        APP_LOGE("lbs init fail ret=%d", ret);
        return -2;
    }

    ret = cm_lbs_location(lbs_cb, NULL);
    if (ret != 0) {
        APP_LOGE("lbs location req fail ret=%d", ret);
        cm_lbs_deinit();
        return -3;
    }

    /* 轮询等待回调完成（SDK 异步，回调上下文非本任务）
     * 超时 = 请求超时 + 5s 余量 */
    uint32_t start = (uint32_t)osKernelGetTickCount();
    uint32_t timeout_tick = APP_MS_TO_TICK(APP_LBS_TIMEOUT_MS + 5000);
    while (!s_lbs_done) {
        if ((uint32_t)osKernelGetTickCount() - start >= timeout_tick) {
            APP_LOGE("lbs wait callback timeout");
            cm_lbs_deinit();
            return -4;
        }
        osDelay(APP_MS_TO_TICK(200));
    }

    if (s_lbs_event != (int)CM_LBS_LOCATION_OK) {
        APP_LOGE("lbs fail event=%d", s_lbs_event);
        return -5;
    }

    double gcj_lon = atof(s_lon_str);
    double gcj_lat = atof(s_lat_str);
    if (gcj_lon == 0.0 || gcj_lat == 0.0) {
        APP_LOGE("lbs invalid lon/lat (%s, %s)", s_lon_str, s_lat_str);
        return -6;
    }

    /* GCJ02 -> WGS84 */
    double wgs_lon, wgs_lat;
    gcj02_to_wgs84(gcj_lon, gcj_lat, &wgs_lon, &wgs_lat);

    /* 填充 app_location_t（与 GPS 字段对齐） */
    memset(out_loc, 0, sizeof(*out_loc));
    out_loc->longitude     = wgs_lon;
    out_loc->latitude      = wgs_lat;
    out_loc->accuracy      = atoi(s_radius_str);   /* 精度半径 m */
    out_loc->satellite_cnt = 0;
    out_loc->speed         = 0.0f;
    out_loc->heading       = 0.0f;
    out_loc->altitude      = 0.0f;
    strncpy(out_loc->source, "LBS", sizeof(out_loc->source) - 1);
    strncpy(out_loc->coord_sys, "WGS84", sizeof(out_loc->coord_sys) - 1);

    APP_LOGI("lbs ok: gcj(%.6f,%.6f)->wgs(%.6f,%.6f) acc=%dm",
             gcj_lon, gcj_lat, wgs_lon, wgs_lat, out_loc->accuracy);
    return 0;
}

#endif /* APP_LBS_ENABLED */
