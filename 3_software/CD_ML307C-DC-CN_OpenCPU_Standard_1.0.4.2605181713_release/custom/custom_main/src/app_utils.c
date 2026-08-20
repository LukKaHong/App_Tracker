/**
 * @file    app_utils.c
 * @brief   通用工具实现
 * @details HMAC-SHA256 通过 lwip/mbedtls 链接库间接调用，
 *          工程已包含 prebuild/libs 中的 libmbedtls.a / libmbedcrypto.a / libmbedx509.a。
 *          若链接缺失，回退到占位实现（仅联调用，正式发布前需保证 HMAC 真实性）。
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "cm_sys.h"
#include "cm_rtc.h"
#include "app_utils.h"
#include "app_log.h"

/* 启用 mbedtls 真实 HMAC-SHA256 实现（third-party/mbedtls 已被编译进 elf） */
#define USE_MBEDTLS_HMAC 1
#if USE_MBEDTLS_HMAC
#include "mbedtls/md.h"
#endif

void app_util_format_rfc3339(uint64_t unix_sec, char *buf, size_t buf_len)
{
    /* 不调用 localtime，直接按 UTC 计算，避免依赖 tz */
    int sec   = (int)(unix_sec % 60);
    int min   = (int)((unix_sec / 60) % 60);
    int hour  = (int)((unix_sec / 3600) % 24);
    /* 计算日期：算法采用 Howard Hinnant 的 days_from_civil */
    long days = (long)(unix_sec / 86400);
    days = days + 719468;
    long era = (days >= 0 ? days : days - 146096) / 146097;
    unsigned long doe = (unsigned long)(days - era * 146097);
    unsigned long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long y = (long)(yoe) + era * 400;
    unsigned long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned mp = (5 * doy + 2) / 153;
    unsigned d  = doy - (153 * mp + 2) / 5 + 1;
    unsigned m  = mp + (mp < 10 ? 3 : -9);
    y = y + (m <= 2 ? 1 : 0);

    if (buf_len < 21) {
        if (buf_len > 0) buf[0] = '\0';
        return;
    }
    snprintf(buf, buf_len, "%04ld-%02u-%02uT%02d:%02d:%02dZ",
             y, m, d, hour, min, sec);
}

void app_util_gen_boot_id(char *buf, size_t buf_len)
{
    uint64_t now = cm_rtc_get_current_time();
    char ts[24] = {0};
    app_util_format_rfc3339(now, ts, sizeof(ts));
    /* 去掉分隔符以缩短：YYYYMMDDHHMMSS */
    char compact[16] = {0};
    int j = 0;
    for (int i = 0; ts[i] && j < (int)sizeof(compact) - 1; i++) {
        if (ts[i] >= '0' && ts[i] <= '9') {
            compact[j++] = ts[i];
        }
    }
    /* 取低 4 位 tick 作为随机部分 */
    uint32_t rnd = (uint32_t)(now & 0xFFFF) ^ 0x1234;
    snprintf(buf, buf_len, "boot_%s_%04lx", compact, (unsigned long)(rnd & 0xFFFF));
}

void app_util_gen_nonce(char *buf, size_t buf_len)
{
    if (buf_len < 17) {
        if (buf_len > 0) buf[0] = '\0';
        return;
    }
    uint64_t now = cm_rtc_get_current_time();
    uint32_t r = (uint32_t)(now * 2654435761u);
    snprintf(buf, buf_len, "n%015lu", (unsigned long)(now ^ r));
}

int app_util_hmac_sha256_hex(const char *key, size_t key_len,
                              const char *msg, size_t msg_len,
                              char *out_hex, size_t out_hex_len)
{
    if (out_hex_len < 65) {
        return -1;
    }

#if USE_MBEDTLS_HMAC
    const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (!info) return -2;
    unsigned char hmac_out[32];
    int ret = mbedtls_md_hmac(info,
                              (const unsigned char *)key, key_len,
                              (const unsigned char *)msg, msg_len,
                              hmac_out);
    if (ret != 0) return ret;
    for (int i = 0; i < 32; i++) {
        snprintf(out_hex + i * 2, 3, "%02x", hmac_out[i]);
    }
    out_hex[64] = '\0';
    return 0;
#else
    /* 占位实现：联调前必须替换为真实 HMAC-SHA256
     * 这里使用 SHA-256 简化版以确保编译通过，但签名不正确 */
    (void)key; (void)key_len; (void)msg_len;
    APP_LOGW("HMAC placeholder used, NOT for production");
    /* 用消息的简单 hash 作为占位输出，长度 64 字节 hex */
    unsigned char digest[32];
    for (int i = 0; i < 32; i++) {
        digest[i] = (unsigned char)(msg[i % msg_len] ^ key[i % key_len] ^ (i * 0x5A));
    }
    for (int i = 0; i < 32; i++) {
        snprintf(out_hex + i * 2, 3, "%02x", digest[i]);
    }
    out_hex[64] = '\0';
    return 0;
#endif
}

int app_util_json_find_string(const char *json, const char *key,
                              char *out, size_t out_len)
{
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(json, pattern);
    if (!p) return -1;
    p += strlen(pattern);
    /* 跳过 : 与空白 */
    while (*p && (*p == ':' || *p == ' ' || *p == '\t')) p++;
    if (*p != '"') return -1;
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i + 1 < out_len) {
        if (*p == '\\' && *(p + 1)) p++;
        out[i++] = *p++;
    }
    out[i] = '\0';
    return (int)i;
}

int app_util_json_find_int(const char *json, const char *key, int *out)
{
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(json, pattern);
    if (!p) return -1;
    p += strlen(pattern);
    while (*p && (*p == ':' || *p == ' ' || *p == '\t')) p++;
    if (*p < '0' || (*p > '9' && *p != '-')) return -1;
    *out = atoi(p);
    return 0;
}

uint32_t app_util_get_tick_ms(void)
{
    return (uint32_t)(cm_rtc_get_current_time() * 1000ULL);
}

void app_util_gen_loc_message_id(const char *imei, const char *boot_id,
                                  uint32_t seq, char *out, size_t out_len)
{
    snprintf(out, out_len, "loc_%s_%s_%06lu", imei, boot_id, (unsigned long)seq);
}
