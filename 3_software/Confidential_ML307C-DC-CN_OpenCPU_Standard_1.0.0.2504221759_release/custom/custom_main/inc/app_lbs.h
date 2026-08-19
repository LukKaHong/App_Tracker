/**
 * @file    app_lbs.h
 * @brief   LBS 基站定位兜底模块（OneOsPos 平台）
 * @details GPS 定位失败（室内/超时）时，通过中移 OneOsPos 平台进行
 *          基站粗定位作为兜底。模组内置 PID，免 Key，共享日配额。
 *          参照 SDK 示例 examples/oneos_lbs 将异步接口封装为同步接口。
 *          注意：内部走 HTTP，需 PS 网络已注册（CEREG=1/5）；
 *          必须在任务上下文调用（禁止中断/timer 回调）。
 */
#ifndef __APP_LBS_H__
#define __APP_LBS_H__

#include <stdint.h>
#include "app_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 同步获取 LBS 基站定位（阻塞直至成功/失败/超时）
 *
 * @param[out] out_loc    定位结果（成功时 source="LBS"）
 * @param[in]  timeout_s  定位超时（秒，范围 0~60）
 *
 * @return 0 成功（out_loc 有效）
 * @return <0 失败，错误码同 cm_lbs_callback_event_e（-1 网络异常 / -2 超时 等）
 */
int app_lbs_get_location(app_location_t *out_loc, int timeout_s);

#ifdef __cplusplus
}
#endif
#endif /* __APP_LBS_H__ */
