/**
 * @file    app_ota.h
 * @brief   APP 整包升级（联调协议 V1 2：属性快照驱动 + MQTT 分片下载 + SHA256 校验
 *          + 五状态上报 + 升级恢复标志）。RPC method=OTA 已废弃（协议 2.2）。
 */
#ifndef __APP_OTA_H__
#define __APP_OTA_H__

#include <stdint.h>
#include <stdbool.h>
#include "cm_os.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===== OTA 数据桥（custom_main.c 侧定义/实现，ota_task 消费）===== */

/* 分片/错误事件位（custom_main.c MQTT 回调置位 g_fw_evt，ota_task 等待） */
#define APP_FW_EVT_CHUNK   0x00000001u
#define APP_FW_EVT_ERROR   0x00000002u

/* 分片/error 事件组（custom_main.c 定义并创建，system_init 中先于 app_ota_init） */
extern osEventFlagsId_t g_fw_evt;

/* ota_task 取走分片槽数据（take 即清 pending；拷贝到调用方私有缓冲，
 * 使槽在写 flash 期间可被回调复用）。无 pending 返回 false */
bool app_main_ota_chunk_take(char *topic_out, int topic_size,
                             uint8_t *buf_out, int buf_size, int *len_out);

/* ===== 应用接口 ===== */

/* 初始化：创建内部事件组并启动常驻 ota_task（system_init 调用一次。
 * 触发源为属性消息，不再有 RPC url 入口） */
int  app_ota_init(void);

/* 属性消息投递（主循环消费 custom_main attr 槽后调用）：
 * topic 为 "v1/devices/me/attributes"（更新通知 = 刷新信号，协议 2.2）
 * 或 "v1/devices/me/attributes/response/{reqId}"（完整快照） */
void app_ota_on_attr(const char *topic, const char *payload, int len);

/* 上报当前版本（联调协议 V1 2.2）：客户端属性 current_fw_* 到
 * v1/devices/me/attributes，同名遥测到 v1/devices/me/telemetry。
 * 每次 MQTT 连接订阅成功后调用 */
void app_ota_report_current_fw(void);

/* 新固件启动自检（MQTT 订阅成功后由主循环调用一次）：
 * 消费持久化恢复标志——本机版本 == resume_version → 上报 UPDATED
 * 并记录任务已完成；不匹配（刷写失败/回滚）→ 上报 FAILED/INSTALL_FAILED，
 * 当前版本不变（协议 2.5） */
void app_ota_boot_resume_check(void);

/* OTA 流程是否活跃（快照等待/下载/校验/安装期间 true；
 * 主循环据此冻结 LP 睡眠与定位周期） */
bool app_ota_is_running(void);

#ifdef __cplusplus
}
#endif
#endif /* __APP_OTA_H__ */
