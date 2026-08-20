/**
 * @file    app_protocol.h
 * @brief   设备-云端协议数据结构定义
 *          详见《DEVICE_CLOUD_PROTOCOL.md》
 */
#ifndef __APP_PROTOCOL_H__
#define __APP_PROTOCOL_H__

#include <stdint.h>
#include <stdbool.h>
#include "app_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===== event_type ===== */
#define APP_EVT_LOCATION        "location"
#define APP_EVT_STATE           "state"
#define APP_EVT_COMMAND_RESULT  "command_result"

/* ===== online_status ===== */
#define APP_STATUS_ONLINE       "ONLINE"
#define APP_STATUS_OFFLINE      "OFFLINE"
#define APP_STATUS_STALE        "STALE"

/* ===== command status ===== */
#define APP_CMD_ACK             "ACKNOWLEDGED"
#define APP_CMD_FAILED          "FAILED"

/* ===== 云端指令 method ===== */
typedef enum {
    APP_CMD_NONE = 0,
    APP_CMD_HIGH_FREQ_START,
    APP_CMD_HIGH_FREQ_STOP,
    APP_CMD_SOUND,
    APP_CMD_LIGHT,
    APP_CMD_LOCATION_FREQ,
    APP_CMD_SHUTDOWN,
    APP_CMD_UNKNOWN,
} app_command_type_e;

/* ===== 定位数据 ===== */
typedef struct {
    double longitude;
    double latitude;
    float  speed;          /* m/s */
    float  heading;        /* 0~360 */
    float  altitude;       /* m */
    int    satellite_cnt;
    int    accuracy;       /* m */
    char   source[8];      /* GPS / LBS / WIFI / MIXED */
    char   coord_sys[8];   /* WGS84 / GCJ02 */
} app_location_t;

/* ===== 设备状态 ===== */
typedef struct {
    char    online_status[16];
    int     battery_level;
    char    firmware_version[16];
    char    network_type[8];
    int     signal_strength;
} app_device_state_t;

/* ===== 离线缓存条目：复用 telemetry payload ===== */
#define APP_OFFLINE_RECORD_PAYLOAD_LEN  (512)
typedef struct {
    char    message_id[APP_MSG_ID_MAX_LEN];
    char    event_time[24];      /* RFC3339 */
    char    payload[APP_OFFLINE_RECORD_PAYLOAD_LEN];
    int     payload_len;
} app_offline_record_t;

/* ===== JSON 构造与解析 API =====
 * 注意：所有 API 使用调用方提供的静态/栈缓冲区，不使用 cJSON malloc/free，
 * 避免与 cmmqtt-m 任务并发堆操作导致堆损坏（newlib malloc 非线程安全） */

/* RPC 解析结果：包含 method、command_id 及常用 params 字段 */
typedef struct {
    char method[32];
    char command_id[64];
    int  duration_seconds;   /* SOUND/LIGHT 持续时长，默认 -1 表示未提供 */
    int  interval_seconds;   /* LOCATION_FREQUENCY 间隔，默认 -1 表示未提供 */
    char mode[16];           /* DEVICE_MODE 模式串：searching/walking/supervise/lowpower/sleep，空表示未提供 */
    char url[256];           /* OTA 升级 URL，空字符串表示未提供 */
} app_rpc_parsed_t;

/* 组装命令结果 JSON 到 out_buf（不使用 malloc）
 * 返回 0 成功，<0 失败；*out_len 为生成的 JSON 长度 */
int app_protocol_build_command_result(const char *imei, const char *command_id,
                                       const char *status,
                                       const char *failure_code,
                                       const char *failed_reason,
                                       char *out_buf, size_t buf_size, int *out_len);

/* 解析 RPC payload（不使用 cJSON malloc/free）
 * 返回 0 成功，<0 失败；结果写入 out->method / out->command_id 等 */
int app_protocol_parse_rpc(const char *payload, int payload_len,
                            app_rpc_parsed_t *out);

int app_protocol_extract_request_id(const char *topic, char *out, size_t out_len);

#ifdef __cplusplus
}
#endif
#endif /* __APP_PROTOCOL_H__ */
