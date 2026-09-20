/**
 * @file    app_ota.c
 * @brief   APP 整包升级（联调协议 V1 2）：属性快照驱动 + MQTT 分片下载
 *          - 触发：attributes 更新通知 = 刷新信号 → 重新请求完整快照；
 *            attributes/response/{reqId} 快照 → 6 项元数据校验（协议 2.3）
 *          - 下载：v2/fw/request/{reqId}/chunk/{N} 顺序请求（4096B 推荐），
 *            严格 EOF 确认，边收边流式 SHA-256，分片 5s 超时 × 3 重试（协议 2.4）
 *          - 状态机：DOWNLOADING → DOWNLOADED → VERIFIED → UPDATING → UPDATED，
 *            失败上报 5 个稳定错误码之一（协议 2.5）
 *          - 任务唯一标识（title+version+checksum）防重复安装；失败任务
 *            本次运行内不自动重试（DOWNLOAD_FAILED 60s 冷却后允许新快照重试，
 *            覆盖断线重连场景）
 *          - UPDATING 前持久化恢复标志（otastate.bin），新固件启动自检通过后
 *            上报 UPDATED；刷写失败/回滚 → INSTALL_FAILED（app_ota_boot_resume_check）
 *          - 低电拒绝（需求 6.4）；升级期间主循环冻结 LP 与定位周期
 *            （custom_main.c 门控，app_ota_is_running）
 *          - 常驻 ota_task（rti 泄漏加固：终身存活，事件驱动，无线程表项泄漏）
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "cm_os.h"
#include "cm_ota.h"
#include "app_log.h"
#include "app_config.h"
#include "app_utils.h"
#include "app_mqtt_client.h"
#include "app_storage.h"
#include "bsp.h"
#include "app_ota.h"

/* ===== 内部事件（s_ota_evt）===== */
#define OTA_EVT_ATTR    0x00000001u   /* 属性消息到达（s_attr_* 有效） */

/* 属性消息缓冲（主循环 app_ota_on_attr 拷入，ota_task 消费）。
 * 拷入后补 NUL 终结：JSON 查找（strstr）按 C 字符串处理 */
#define OTA_ATTR_BUF_SIZE   1024
static volatile bool s_attr_pending = false;
static char s_attr_topic[80] = {0};
static char s_attr_buf[OTA_ATTR_BUF_SIZE] = {0};

/* 分片数据私有缓冲：从 custom_main 槽 take 拷入，写 flash 期间槽可被
 * 回调复用（单槽丢新语义不受影响）。大小与 custom_main FW_CHUNK_BUF_SIZE 一致 */
#define OTA_CHUNK_BUF_SIZE  (4096 + 64)
static uint8_t s_chunk_buf[OTA_CHUNK_BUF_SIZE] = {0};
static char    s_chunk_topic[80] = {0};
static int     s_chunk_len = 0;

/* 快照请求会话：当前等待的 requestId（<=0 表示未在等待） */
static volatile int32_t s_snapshot_reqid = -1;

/* OTA 流程活跃标志（LP/定位冻结门控） */
static volatile bool s_running = false;
static osEventFlagsId_t s_ota_evt = NULL;

/* 失败任务记忆（协议 2.5：同一失败任务不得立即无限循环重试）：
 * checksum 型永久失败（元数据/校验/安装类，本次运行内跳过同任务）*/
static char s_fatal_failed_checksum[65] = {0};
/* 下载失败（断线/超时等环境类）：60s 冷却后允许新快照重试（协议 2.4 第 8 条
 * 断线重连后从第 0 块重新下载）*/
static char s_dl_fail_checksum[65] = {0};
static uint32_t s_dl_fail_tick = 0;
#define OTA_DL_FAIL_COOLDOWN_MS  (60u * 1000u)

/* ===== 元数据 ===== */
typedef struct {
    char     title[48];
    char     version[64];
    char     algo[24];
    char     checksum[65];       /* 64 hex + '\0' */
    uint32_t size;
} ota_meta_t;

bool app_ota_is_running(void) { return s_running; }

/* ===== 小工具 ===== */

/* 忽略大小写字符串相等（checksum/algo 比较） */
static bool ota_ieq(const char *a, const char *b)
{
    if (!a || !b) return false;
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
        a++; b++;
    }
    return (*a == '\0') && (*b == '\0');
}

/* 64 位 hex 串校验（协议 2.3：fw_checksum 64 位 SHA-256 hex） */
static bool ota_hex64_valid(const char *s)
{
    if (s == NULL || strlen(s) != 64) return false;
    for (int i = 0; i < 64; i++) {
        if (!isxdigit((unsigned char)s[i])) return false;
    }
    return true;
}

/* 校验算法规范化（去分隔符 + 大写化）后必须为 SHA256（协议 2.3） */
static bool ota_algo_is_sha256(const char *algo)
{
    if (algo == NULL) return false;
    char norm[24] = {0};
    int j = 0;
    for (int i = 0; algo[i] != '\0' && j < (int)sizeof(norm) - 1; i++) {
        if (algo[i] == '-' || algo[i] == '_' || algo[i] == ' ') continue;
        norm[j++] = (char)toupper((unsigned char)algo[i]);
    }
    norm[j] = '\0';
    return strcmp(norm, "SHA256") == 0;
}

/* 属性 topic 分类：true = 快照响应（解析出 reqId） */
static bool ota_attr_is_response(const char *topic, int32_t *reqid)
{
    /* "v1/devices/me/attributes/response/{reqId}"，前缀长 34 */
    if (strncmp(topic, "v1/devices/me/attributes/response/", 34) == 0) {
        *reqid = (int32_t)strtol(topic + 34, NULL, 10);
        return true;
    }
    return false;
}

/* 低电检查（需求 6.4 V1.30）：SOC 低于超低电阈值且非充电 → 拒绝。
 * SOC 读不到（无电池台架/ADC 异常）不阻塞；充电中放行 */
static bool ota_battery_ok(void)
{
    int mv = 0, soc = -1;
    if (bsp_battery_read(&mv, &soc) == 0 && soc >= 0 &&
        soc < APP_SUPER_LOW_BATTERY && !bsp_charge_is_charging()) {
        return false;
    }
    return true;
}

/* 升级区安全擦除（失败再试一次，官方 fota demo 同款） */
static void ota_erase_safe(void)
{
    if (cm_ota_firmware_erase() != 0) {
        (void)cm_ota_firmware_erase();
    }
}

/* 五状态/失败上报到 telemetry（协议 2.5 payload 格式） */
static void ota_report_state(const char *state, const char *fw_error)
{
    char json[192];
    int len = snprintf(json, sizeof(json),
                       "{\"fw_state\":\"%s\",\"fw_error\":\"%s\","
                       "\"current_fw_title\":\"%s\",\"current_fw_version\":\"%s\"}",
                       state, fw_error, APP_MODEL_CODE, APP_FIRMWARE_VERSION);
    (void)app_mqtt_publish_telemetry(json, len);
}

/* 失败收尾：上报 FAILED + 记录失败任务。
 * fatal=true：本次运行内永久跳过同任务（元数据/校验/安装类失败）；
 * fatal=false：60s 冷却后允许新快照重试（下载类失败，覆盖断线恢复） */
static void ota_fail(const ota_meta_t *meta, const char *fw_error, bool fatal)
{
    APP_LOGE("ota FAILED: %s", fw_error);
    if (meta != NULL && meta->checksum[0] != '\0') {
        if (fatal) {
            strncpy(s_fatal_failed_checksum, meta->checksum,
                    sizeof(s_fatal_failed_checksum) - 1);
            s_fatal_failed_checksum[sizeof(s_fatal_failed_checksum) - 1] = '\0';
        } else {
            strncpy(s_dl_fail_checksum, meta->checksum,
                    sizeof(s_dl_fail_checksum) - 1);
            s_dl_fail_checksum[sizeof(s_dl_fail_checksum) - 1] = '\0';
            s_dl_fail_tick = (uint32_t)osKernelGetTickCount();
        }
    }
    ota_report_state("FAILED", fw_error);
}

/* ===== 快照请求与等待（协议 2.2）===== */

/* 返回 true：s_attr_* 中已是 reqId 匹配的快照响应（已消费 s_attr_pending） */
static bool ota_request_snapshot(void)
{
    for (uint32_t attempt = 0; attempt <= APP_OTA_SNAPSHOT_RETRY_MAX; attempt++) {
        if (attempt > 0) {
            osDelay(APP_MS_TO_TICK(2000));   /* 重试间隔 */
        }
        s_snapshot_reqid = app_util_next_request_id();
        char topic[64];
        snprintf(topic, sizeof(topic), "%s%d", APP_MQTT_TOPIC_ATTR_REQ,
                 (int)s_snapshot_reqid);
        char payload[128];
        int plen = snprintf(payload, sizeof(payload),
                            "{\"sharedKeys\":\"%s\"}", APP_OTA_SHARED_KEYS);
        if (app_mqtt_publish_topic(topic, payload, plen) != 0) {
            APP_LOGW("ota snapshot publish fail (attempt %u)", (unsigned)attempt);
            continue;
        }
        /* 等响应/通知（主循环 app_ota_on_attr 投递）。
         * 通知也会唤醒：视为刷新信号重新请求（消耗一次重试，有界） */
        uint32_t flags = osEventFlagsWait(s_ota_evt, OTA_EVT_ATTR, osFlagsWaitAny,
                                          APP_MS_TO_TICK(APP_OTA_SNAPSHOT_TIMEOUT_MS));
        if ((flags & 0x80000000u) != 0u || !s_attr_pending) {
            APP_LOGW("ota snapshot wait timeout (attempt %u)", (unsigned)attempt);
            continue;
        }
        int32_t rid = 0;
        if (ota_attr_is_response(s_attr_topic, &rid) && rid == s_snapshot_reqid) {
            /* 保持 pending=true：解析期间主循环侧暂停投递（丢新），
             * 防快照缓冲 s_attr_buf 被并发改写；任务循环末尾统一清槽 */
            return true;
        }
        APP_LOGI("ota attr notify while waiting, re-request snapshot");
        s_attr_pending = false;
    }
    APP_LOGE("ota snapshot not available after retries");
    return false;
}

/* ===== 元数据解析与校验（协议 2.3）===== */

/* 返回 true 校验通过；失败时已上报对应错误码 */
static bool ota_meta_check(ota_meta_t *meta)
{
    const char *json = s_attr_buf;
    char url_probe[8] = {0};
    /* 响应字段可能位于根 object 或 shared object（协议 2.2）：
     * json_find 全文匹配同名 key，天然兼容两种位置 */
    bool have_url = (app_util_json_find_string(json, "fw_url",
                                               url_probe, sizeof(url_probe)) >= 0);
    (void)app_util_json_find_string(json, "fw_title", meta->title, sizeof(meta->title));
    (void)app_util_json_find_string(json, "fw_version", meta->version, sizeof(meta->version));
    (void)app_util_json_find_string(json, "fw_checksum", meta->checksum, sizeof(meta->checksum));
    (void)app_util_json_find_string(json, "fw_checksum_algorithm", meta->algo, sizeof(meta->algo));
    int sz = 0;
    bool have_size = (app_util_json_find_int(json, "fw_size", &sz) == 0);

    /* 算法单独判定（协议 2.3：算法错误上报独立错误码） */
    if (!ota_algo_is_sha256(meta->algo)) {
        ota_fail(meta, "UNSUPPORTED_CHECKSUM_ALGORITHM", true);
        return false;
    }
    if (meta->title[0] == '\0' ||
        strcmp(meta->title, APP_MODEL_CODE) != 0 ||
        meta->version[0] == '\0' ||
        strlen(meta->version) > 64 ||
        strcmp(meta->version, APP_FIRMWARE_VERSION) == 0 ||
        !have_size || sz <= 0 || (uint32_t)sz > APP_OTA_FW_SIZE_MAX ||
        !ota_hex64_valid(meta->checksum) ||
        (have_url && url_probe[0] != '\0')) {
        ota_fail(meta, "INVALID_FIRMWARE_METADATA", true);
        return false;
    }
    meta->size = (uint32_t)sz;

    /* 升级区容量（存储条件，协议 2.5 第 3 条/2.3 大小越界） */
    int32_t cap = cm_ota_get_capacity();
    if (cap > 0 && (int32_t)meta->size > cap) {
        APP_LOGE("ota pkg %u > ota area capacity %d", meta->size, (int)cap);
        ota_fail(meta, "INVALID_FIRMWARE_METADATA", true);
        return false;
    }
    return true;
}

/* ===== 分片下载（协议 2.4）===== */

/* 下载分片并流式写入升级区 + SHA-256。
 * 返回 0 成功（EOF 已严格确认，sha 已 finish 进 out_hex）；
 * 失败返回 -1（err 输出 DOWNLOAD_FAILED/INSTALL_FAILED/CHECKSUM_MISMATCH） */
static int ota_download_chunks(const ota_meta_t *meta, char *out_hex, const char **err)
{
    app_util_sha256_t sha;
    app_util_sha256_init(&sha);

    int32_t rid = app_util_next_request_id();
    uint32_t received = 0;
    uint32_t chunk_idx = 0;
    bool eof_done = false;
    int ret = -1;
    *err = "DOWNLOAD_FAILED";

    while (!eof_done) {
        uint32_t remain = meta->size - received;      /* received <= size 恒成立 */
        /* 收满后 remain=0 → 请求 payload "0" → 服务器回 0 字节 EOF（协议 2.4 第 5 条） */
        uint32_t expect = (remain > APP_OTA_CHUNK_SIZE) ? APP_OTA_CHUNK_SIZE : remain;

        char req_topic[64];
        snprintf(req_topic, sizeof(req_topic), "%s%d/chunk/%u",
                 APP_MQTT_TOPIC_FW_REQ, (int)rid, (unsigned)chunk_idx);
        char req_pay[12];
        int req_len = snprintf(req_pay, sizeof(req_pay), "%u", (unsigned)expect);

        char exp_topic[80];
        snprintf(exp_topic, sizeof(exp_topic), "v2/fw/response/%d/chunk/%u",
                 (int)rid, (unsigned)chunk_idx);

        bool got = false;
        for (uint32_t retry = 0; retry <= APP_OTA_CHUNK_RETRY_MAX && !got; retry++) {
            if (retry > 0) {
                APP_LOGW("ota chunk %u retry %u/%u", (unsigned)chunk_idx,
                         (unsigned)retry, (unsigned)APP_OTA_CHUNK_RETRY_MAX);
            }
            if (app_mqtt_publish_topic(req_topic, req_pay, req_len) != 0) {
                osDelay(APP_MS_TO_TICK(500));
                continue;
            }
            uint32_t flags = osEventFlagsWait(g_fw_evt,
                                              APP_FW_EVT_CHUNK | APP_FW_EVT_ERROR,
                                              osFlagsWaitAny,
                                              APP_MS_TO_TICK(APP_OTA_CHUNK_TIMEOUT_MS));
            if ((flags & 0x80000000u) != 0u) {
                continue;                     /* 超时 → 重试 */
            }
            if ((flags & APP_FW_EVT_ERROR) != 0u) {
                APP_LOGE("ota v2/fw/error received");
                goto out;                     /* DOWNLOAD_FAILED */
            }
            if ((flags & APP_FW_EVT_CHUNK) == 0u) {
                continue;
            }
            /* take 到私有缓冲：槽立即可被回调复用 */
            if (!app_main_ota_chunk_take(s_chunk_topic, sizeof(s_chunk_topic),
                                         s_chunk_buf, sizeof(s_chunk_buf),
                                         &s_chunk_len)) {
                continue;
            }
            /* topic 严格匹配（协议 2.4 第 6 条：乱序包 → 失败） */
            if (strcmp(s_chunk_topic, exp_topic) != 0) {
                APP_LOGE("ota chunk topic mismatch: %s exp %s",
                         s_chunk_topic, exp_topic);
                goto out;
            }
            if (s_chunk_len == 0) {
                /* 空包：仅收满后允许（EOF）；提前空包 = 失败（协议 2.4 第 6 条） */
                if (received == meta->size) {
                    eof_done = true;
                    got = true;
                    break;
                }
                APP_LOGE("ota premature EOF at %u/%u", (unsigned)received,
                         (unsigned)meta->size);
                goto out;
            }
            /* 普通分片长度必须等于 min(chunk_size, fw_size - received)
             * （协议 2.4 第 4 条）：短包/超量非空包均失败 */
            if ((uint32_t)s_chunk_len != expect) {
                APP_LOGE("ota chunk len %d != expect %u", s_chunk_len,
                         (unsigned)expect);
                goto out;
            }
            got = true;
        }
        if (!got) {
            APP_LOGE("ota chunk %u retries exhausted", (unsigned)chunk_idx);
            goto out;                         /* DOWNLOAD_FAILED */
        }
        if (eof_done) break;

        /* 流式 SHA-256（协议 2.4 第 3 条：每收到一块立即更新） */
        if (app_util_sha256_update(&sha, s_chunk_buf, (size_t)s_chunk_len) != 0) {
            *err = "INSTALL_FAILED";
            goto out;
        }
        /* 分段写入升级区（尾部累加写，失败重试一次） */
        int w = (int)cm_ota_firmware_write((const char *)s_chunk_buf,
                                           (size_t)s_chunk_len);
        if (w != 0) {
            APP_LOGW("ota write fail:%d, retry", w);
            w = (int)cm_ota_firmware_write((const char *)s_chunk_buf,
                                           (size_t)s_chunk_len);
        }
        if (w != 0) {
            APP_LOGE("ota write fail at offset=%u", (unsigned)received);
            *err = "INSTALL_FAILED";
            goto out;
        }
        received += (uint32_t)s_chunk_len;
        chunk_idx++;
        if ((chunk_idx % 16u) == 0u) {
            APP_LOGI("ota %u/%u (%u%%)", (unsigned)received, (unsigned)meta->size,
                     (unsigned)(received * 100u / meta->size));
        }
    }

    /* 写入总量复核（累加写 + 严格 EOF 双重确认，防静默丢包） */
    if (cm_ota_get_written_size() != (int32_t)meta->size) {
        APP_LOGE("ota written=%d != size=%u",
                 (int)cm_ota_get_written_size(), (unsigned)meta->size);
        goto out;
    }
    if (app_util_sha256_finish_hex(&sha, out_hex) != 0) {
        *err = "INSTALL_FAILED";
        goto out;
    }
    ret = 0;

out:
    app_util_sha256_free(&sha);
    return ret;
}

/* ===== 单次 OTA 流程（ota_task 的执行单元）===== */

static void ota_process_snapshot(void)
{
    ota_meta_t meta = {0};

    if (!ota_meta_check(&meta)) {
        return;   /* 已上报对应错误码 */
    }

    /* 任务唯一标识去重（协议 2.3：title+version+checksum，同一任务不得重复安装） */
    app_ota_persist_t st;
    memset(&st, 0, sizeof(st));
    bool have_state = (app_storage_load_ota_state(&st) == 0);
    if (have_state && st.last_done_valid &&
        strcmp(st.done_title, meta.title) == 0 &&
        strcmp(st.done_version, meta.version) == 0 &&
        ota_ieq(st.done_checksum, meta.checksum)) {
        APP_LOGI("ota task %s already installed, skip", meta.version);
        return;
    }
    /* 本次运行内致命失败的任务不自动重试（协议 2.5） */
    if (s_fatal_failed_checksum[0] != '\0' &&
        ota_ieq(s_fatal_failed_checksum, meta.checksum)) {
        APP_LOGI("ota task failed fatally before, wait new snapshot/reboot");
        return;
    }
    /* 下载失败冷却（断线等环境类：60s 后允许新快照重新下载，协议 2.4 第 8 条） */
    if (s_dl_fail_checksum[0] != '\0' &&
        ota_ieq(s_dl_fail_checksum, meta.checksum)) {
        uint32_t now = (uint32_t)osKernelGetTickCount();
        uint32_t dt = now - s_dl_fail_tick;    /* tick 回绕安全（无符号减法） */
        if (dt < OTA_DL_FAIL_COOLDOWN_MS &&
            s_dl_fail_tick != 0 && now >= s_dl_fail_tick) {
            APP_LOGI("ota task download-failed %ums ago, cooldown", (unsigned)dt);
            return;
        }
    }
    /* 低电前置（需求 6.4）：拒绝不构成固件错误，不上报 FAILED，
     * 等待下一次快照/通知触发（充电后可通过） */
    if (!ota_battery_ok()) {
        APP_LOGE("ota rejected: low battery, wait next snapshot");
        return;
    }

    ota_report_state("DOWNLOADING", "");
    APP_LOGI("ota start: %s %s %uB (%u chunks of %uB)", meta.title, meta.version,
             (unsigned)meta.size,
             (unsigned)((meta.size + APP_OTA_CHUNK_SIZE - 1) / APP_OTA_CHUNK_SIZE),
             (unsigned)APP_OTA_CHUNK_SIZE);

    if (cm_ota_init() != 0) {
        ota_erase_safe();
        ota_fail(&meta, "INSTALL_FAILED", true);
        return;
    }
    ota_erase_safe();                     /* 从第 0 块全量写，先清残留 */
    cm_ota_set_otasize((int)meta.size);

    char calc_hex[65] = {0};
    const char *dl_err = "DOWNLOAD_FAILED";
    if (ota_download_chunks(&meta, calc_hex, &dl_err) != 0) {
        ota_erase_safe();
        ota_fail(&meta, dl_err, (strcmp(dl_err, "DOWNLOAD_FAILED") != 0));
        return;
    }

    /* 严格 EOF 已确认 → DOWNLOADED（协议 2.5 第 1 条） */
    ota_report_state("DOWNLOADED", "");
    APP_LOGI("ota downloaded %uB, sha=%s", (unsigned)meta.size, calc_hex);

    /* SHA-256 终比 → VERIFIED（协议 2.5 第 2 条） */
    if (!ota_ieq(calc_hex, meta.checksum)) {
        APP_LOGE("ota sha mismatch: calc=%s exp=%s", calc_hex, meta.checksum);
        ota_erase_safe();
        ota_fail(&meta, "CHECKSUM_MISMATCH", true);
        return;
    }
    ota_report_state("VERIFIED", "");

    /* 电量再确认 + 持久化恢复标志 → UPDATING（协议 2.5 第 3 条） */
    if (!ota_battery_ok()) {
        ota_erase_safe();
        ota_fail(&meta, "INSTALL_FAILED", true);
        return;
    }
    /* st 已含 last_done（读取失败则 last_done 位为 0，不破坏历史记录） */
    st.resume_pending = 1;
    strncpy(st.resume_title, meta.title, sizeof(st.resume_title) - 1);
    strncpy(st.resume_version, meta.version, sizeof(st.resume_version) - 1);
    strncpy(st.resume_checksum, meta.checksum, sizeof(st.resume_checksum) - 1);
    st.resume_size = meta.size;
    if (app_storage_save_ota_state(&st) != 0) {
        APP_LOGE("ota resume persist fail");
        ota_erase_safe();
        ota_fail(&meta, "INSTALL_FAILED", true);
        return;
    }
    ota_report_state("UPDATING", "");

    /* 写入备用分区并切换启动分区（cm_ota_upgrade 内部校验+重启，
     * 成功不返回） */
    if (cm_ota_upgrade() == 0) {
        APP_LOGI("ota upgrade triggered, rebooting");
        return;
    }
    APP_LOGE("ota upgrade trigger fail (pkg verify fail?), old fw keeps running");
    /* 升级未触发：恢复标志必须清除（新固件不会出现，自检无从上报） */
    st.resume_pending = 0;
    (void)app_storage_save_ota_state(&st);
    ota_erase_safe();
    ota_fail(&meta, "INSTALL_FAILED", true);
}

static void ota_run_once(void)
{
    /* 分类本条属性消息（协议 2.2） */
    int32_t rid = 0;
    if (ota_attr_is_response(s_attr_topic, &rid)) {
        if (s_snapshot_reqid <= 0 || rid != s_snapshot_reqid) {
            APP_LOGI("ota stale snapshot resp rid=%d (wait %d), drop",
                     (int)rid, (int)s_snapshot_reqid);
            s_attr_pending = false;
            return;
        }
        /* 快照处理期间保持 pending=true（主循环侧丢新，防并发改写
         * s_attr_buf）；任务循环末尾统一清槽 */
        ota_process_snapshot();
        return;
    }
    /* attributes 更新通知 = 刷新信号：重新请求完整快照（协议 2.2），
     * 不得把多次通知中的字段拼成固件包 */
    if (strcmp(s_attr_topic, APP_MQTT_TOPIC_ATTR_SUB) == 0) {
        s_attr_pending = false;
        if (ota_request_snapshot()) {
            ota_process_snapshot();
        }
        return;
    }
    APP_LOGW("ota unknown attr topic: %s", s_attr_topic);
    s_attr_pending = false;
}

/* 常驻任务（rti 泄漏加固 2026-09-15：终身存活，事件驱动，无线程表项泄漏） */
static void ota_task(void *arg)
{
    (void)arg;
    for (;;) {
        (void)osEventFlagsWait(s_ota_evt, OTA_EVT_ATTR, osFlagsWaitAny,
                               osWaitForever);
        if (!s_attr_pending) continue;
        s_running = true;    /* 流程活跃（含快照等待期）：冻结 LP/定位 */
        ota_run_once();
        s_attr_pending = false;   /* 防御：任何出口都释放槽 */
        s_running = false;
    }
}

int app_ota_init(void)
{
    if (s_ota_evt != NULL) {
        return 0;   /* 已初始化 */
    }
    osEventFlagsAttr_t eattr = {0};
    eattr.name = "ota_evt";
    s_ota_evt = osEventFlagsNew(&eattr);
    if (s_ota_evt == NULL) {
        APP_LOGE("ota evt create fail");
        return -1;
    }
    osThreadAttr_t tattr = {0};
    tattr.name = "ota_task";
    tattr.stack_size = 6 * 1024;
    tattr.priority = osPriorityBelowNormal1;   /* SDK 1.0.4 应用层优先级体系 */
    if (osThreadNew(ota_task, NULL, &tattr) == NULL) {
        APP_LOGE("ota task create fail");
        return -2;
    }
    APP_LOGI("ota task started (snapshot+chunk mode)");
    return 0;
}

void app_ota_on_attr(const char *topic, const char *payload, int len)
{
    if (topic == NULL || payload == NULL || len <= 0) return;
    if (len >= OTA_ATTR_BUF_SIZE - 1) {   /* 预留 1 字节 NUL 终结 */
        APP_LOGW("ota attr too big:%d, drop", len);
        return;
    }
    if (s_attr_pending) {
        APP_LOGW("ota attr busy, drop");   /* 单槽丢新：ota_task 消费慢时保新快照 */
        return;
    }
    strncpy(s_attr_topic, topic, sizeof(s_attr_topic) - 1);
    s_attr_topic[sizeof(s_attr_topic) - 1] = '\0';
    memcpy(s_attr_buf, payload, (size_t)len);
    s_attr_buf[len] = '\0';                /* JSON 查找需要 NUL 终结 */
    s_attr_pending = true;
    if (s_ota_evt != NULL) {
        (void)osEventFlagsSet(s_ota_evt, OTA_EVT_ATTR);
    }
}

void app_ota_report_current_fw(void)
{
    char json[128];
    int len = snprintf(json, sizeof(json),
                       "{\"current_fw_title\":\"%s\",\"current_fw_version\":\"%s\"}",
                       APP_MODEL_CODE, APP_FIRMWARE_VERSION);
    /* 客户端属性 + 同名遥测（联调协议 V1 2.2） */
    (void)app_mqtt_publish_topic(APP_MQTT_TOPIC_ATTR_SUB, json, len);
    (void)app_mqtt_publish_telemetry(json, len);
}

void app_ota_boot_resume_check(void)
{
    app_ota_persist_t st;
    if (app_storage_load_ota_state(&st) != 0 || !st.resume_pending) {
        return;   /* 无待自检恢复信息 */
    }

    if (strcmp(APP_FIRMWARE_VERSION, st.resume_version) == 0) {
        /* 自检通过（本函数由"MQTT 已连接 + 订阅完成"后的主循环调用）：
         * 新固件 = 目标版本 → UPDATED + 更新当前版本属性（协议 2.5 第 5/6 条） */
        APP_LOGI("ota resume: fw %s == target, self-test pass", APP_FIRMWARE_VERSION);
        app_ota_report_current_fw();
        char json[192];
        int len = snprintf(json, sizeof(json),
                           "{\"fw_state\":\"UPDATED\",\"fw_error\":\"\","
                           "\"current_fw_title\":\"%s\",\"current_fw_version\":\"%s\"}",
                           APP_MODEL_CODE, APP_FIRMWARE_VERSION);
        (void)app_mqtt_publish_telemetry(json, len);
        /* 记录任务已完成（防重复安装）+ 清恢复标志 */
        st.resume_pending = 0;
        st.last_done_valid = 1;
        strncpy(st.done_title, st.resume_title, sizeof(st.done_title) - 1);
        strncpy(st.done_version, st.resume_version, sizeof(st.done_version) - 1);
        strncpy(st.done_checksum, st.resume_checksum, sizeof(st.done_checksum) - 1);
        (void)app_storage_save_ota_state(&st);
    } else {
        /* 版本不匹配：刷写失败/启动回滚 → INSTALL_FAILED，当前版本不变
         * （协议 2.5：下载、校验或安装失败时不得修改当前版本） */
        APP_LOGE("ota resume: fw %s != target %s, INSTALL_FAILED",
                 APP_FIRMWARE_VERSION, st.resume_version);
        char json[192];
        int len = snprintf(json, sizeof(json),
                           "{\"fw_state\":\"FAILED\",\"fw_error\":\"INSTALL_FAILED\","
                           "\"current_fw_title\":\"%s\",\"current_fw_version\":\"%s\"}",
                           APP_MODEL_CODE, APP_FIRMWARE_VERSION);
        (void)app_mqtt_publish_telemetry(json, len);
        st.resume_pending = 0;
        (void)app_storage_save_ota_state(&st);
    }
}
