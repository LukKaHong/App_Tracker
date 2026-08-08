/**
 * @file    app_lbs.h
 * @brief   LBS 基站定位封装（OneOSPOS 平台）
 *          作为 GPS 超时回退，提供粗略位置。
 *          cm_lbs_location 为异步接口，此处封装为同步阻塞等待，
 *          返回时已完成 GCJ02->WGS84 坐标转换。
 */
#ifndef __APP_LBS_H__
#define __APP_LBS_H__

#include "app_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 同步获取一次 LBS 基站定位
 * @param out_loc 输出定位结果（成功时填充 source="LBS", coord_sys="WGS84"）
 * @return 0 成功；负值失败
 *             -1 参数错误
 *             -2 初始化失败
 *             -3 发起请求失败
 *             -4 等待回调超时
 *             -5 定位失败（网络/KEY/配额等）
 *             -6 经纬度无效
 * @note 该函数阻塞调用线程约 APP_LBS_TIMEOUT_MS，需在独立任务中调用
 */
int app_lbs_get_location(app_location_t *out_loc);

#ifdef __cplusplus
}
#endif
#endif /* __APP_LBS_H__ */
