/**
 * @file    app_lbs.h
 * @brief   LBS & WiFi 原始参数采集上报（需求 2.2）
 * @details 设备仅采集基站/WiFi 原始参数上报平台，由平台调高德解算坐标。
 *          上报格式（高德智能硬件定位字段名）：
 *            bts     = "mcc,mnc,lac,cellid,signal"           服务小区
 *            nearbts = "mcc,mnc,lac,cellid,signal|..."       邻区列表（| 分隔）
 *            macs    = "aa:bb:cc:dd:ee:ff,rssi|..."          WiFi 列表（| 分隔）
 *          字段含义见需求 2.2：mcc/mnc/lac(TAC)/cellid(ECI)/signal(负数 dBm)、
 *          mac(冒号分隔)/signal(RSSI 负数 dBm)
 *
 *          WiFi 扫描期间模组掉网（CFUN=5），扫描后恢复（CFUN=1）并等待
 *          网络注册。扫描受最小间隔限制（默认 5 分钟）以降低功耗与掉网影响。
 */
#ifndef __APP_LBS_H__
#define __APP_LBS_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化（创建内部同步资源） */
int app_lbs_init(void);

/* 注入 IMEI / boot_id（system_init 时由 custom_main 调用，用于上报字段） */
void app_lbs_set_imei(const char *imei);
void app_lbs_set_boot_id(const char *boot_id);

/* 触发一次 LBS&WiFi 采集（独立任务异步执行，不阻塞主循环）
 * force_wifi = true 时忽略 WiFi 扫描最小间隔限制
 * report      = true 采集后上报平台；false 仅采集不上报
 * 返回 0=已触发，<0=失败（任务正忙等） */
int app_lbs_trigger(bool force_wifi, bool report);

/* 采集上报任务是否正在运行 */
bool app_lbs_is_running(void);

/* 查询最近一次采集缓存的服务小区 bts 字符串（"mcc,mnc,lac,cellid,signal"）
 * 无缓存返回 NULL。仅用于日志/调试显示 */
const char *app_lbs_get_cached_bts(void);

#ifdef __cplusplus
}
#endif
#endif /* __APP_LBS_H__ */
