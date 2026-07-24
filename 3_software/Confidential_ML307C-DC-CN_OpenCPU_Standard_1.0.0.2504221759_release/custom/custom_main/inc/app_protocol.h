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

/* ===== JSON 构造与解析 API ===== */
struct cJSON;
typedef struct cJSON cJSON;

int app_protocol_build_location(const char *imei, const char *boot_id,
                                 uint32_t seq, const app_location_t *loc,
                                 int battery_level, int signal_strength,
                                 bool is_offline,
                                 char **out_json, int *out_len,
                                 char *out_msgid, size_t msgid_len,
                                 char *out_event_time, size_t et_len);

int app_protocol_build_state(const char *imei, const char *online_status,
                              int battery_level, const char *firmware_version,
                              int signal_strength, int charging_status,
                              char **out_json, int *out_len);

int app_protocol_build_command_result(const char *imei, const char *command_id,
                                       const char *status,
                                       const char *failure_code,
                                       const char *failed_reason,
                                       char **out_json, int *out_len);

int app_protocol_parse_rpc(const char *payload, int payload_len,
                            char **out_method, char *out_command_id, size_t cmd_id_len,
                            cJSON **out_params);

int app_protocol_extract_request_id(const char *topic, char *out, size_t out_len);

#ifdef __cplusplus
}
#endif
#endif /* __APP_PROTOCOL_H__ */
