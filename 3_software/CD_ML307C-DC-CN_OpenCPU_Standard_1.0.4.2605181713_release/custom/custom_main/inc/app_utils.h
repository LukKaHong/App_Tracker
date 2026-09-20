/**
 * @file    app_utils.h
 * @brief   通用工具：时间戳格式化、随机串生成、HMAC-SHA256、JSON 辅助
 */
#ifndef __APP_UTILS_H__
#define __APP_UTILS_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "mbedtls/sha256.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 把 UNIX 时间戳格式化为 RFC3339 UTC 字符串，例如 "2026-07-21T10:00:00Z"
 * buf_len 至少 21 字节 */
void app_util_format_rfc3339(uint64_t unix_sec, char *buf, size_t buf_len);

/* 生成 boot_id：基于启动时间 + 随机数，格式 "boot_YYYYMMDDHHMMSS_xxx" */
void app_util_gen_boot_id(char *buf, size_t buf_len);

/* 生成 nonce 随机字符串，长度 16 */
void app_util_gen_nonce(char *buf, size_t buf_len);

/* 计算 HMAC-SHA256，输出 lowercase hex（65 字节含'\0'） */
int app_util_hmac_sha256_hex(const char *key, size_t key_len,
                              const char *msg, size_t msg_len,
                              char *out_hex, size_t out_hex_len);

/* ===== 流式 SHA-256（联调协议 V1 2.4：OTA 分片边收边算）===== */
typedef struct {
    mbedtls_sha256_context ctx;
} app_util_sha256_t;

/* 初始化/释放上下文（free 必须与 init 配对，可在 finish 之后调用） */
void app_util_sha256_init(app_util_sha256_t *c);
void app_util_sha256_free(app_util_sha256_t *c);
/* 追加数据（任意长度，可多次调用）；返回 0 成功 */
int  app_util_sha256_update(app_util_sha256_t *c, const void *data, size_t len);
/* 结束并输出 lowercase hex（out_hex 至少 65 字节含'\0'）；返回 0 成功 */
int  app_util_sha256_finish_hex(app_util_sha256_t *c, char *out_hex);

/* 生成 OTA 用 requestId（联调协议 V1 2.2）：十进制 1..2147483647，
 * 进程内递增、达上限回绕到 1。仅 ota_task 单线程调用，无锁 */
int32_t app_util_next_request_id(void);

/* 简单 JSON 字段值查找：在 json 中查找 "key"，返回对应字符串值
 * 仅支持扁平 JSON，不支持嵌套；返回值长度不含引号 */
int app_util_json_find_string(const char *json, const char *key,
                              char *out, size_t out_len);

/* 简单 JSON 字段值查找：整数 */
int app_util_json_find_int(const char *json, const char *key, int *out);

/* 模组 IMEI 字符串转 boot_id 时使用 */
uint32_t app_util_get_tick_ms(void);

/* 生成 message_id：loc_{imei}_{boot_id}_{sequence_no} */
void app_util_gen_loc_message_id(const char *imei, const char *boot_id,
                                  uint32_t seq, char *out, size_t out_len);

#ifdef __cplusplus
}
#endif
#endif /* __APP_UTILS_H__ */
