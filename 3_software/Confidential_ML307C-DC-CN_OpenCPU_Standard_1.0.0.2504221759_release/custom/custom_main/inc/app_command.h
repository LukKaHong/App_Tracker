/**
 * @file    app_command.h
 * @brief   云端 RPC 指令解析与分发
 */
#ifndef __APP_COMMAND_H__
#define __APP_COMMAND_H__

#include <stdint.h>
#include <stdbool.h>
#include "app_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 解析 RPC payload 并执行对应动作；返回 command 结果状态（ACK/FAILED） */
void app_command_handle(const char *topic, const char *payload, int payload_len);

/* 处理完成后回传 command_result（业务结果） */
void app_command_send_result(const char *command_id, const char *status,
                              const char *failure_code, const char *failed_reason);

/* 注入 IMEI（用于 command_result message_id 生成） */
void app_command_set_imei(const char *imei);

#ifdef __cplusplus
}
#endif
#endif /* __APP_COMMAND_H__ */
