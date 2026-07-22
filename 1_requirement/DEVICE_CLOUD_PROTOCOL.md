# 宠物定位器设备端云端通信协议 V1

状态：设备端联调稿
读者：固件开发、模组通信开发、产测工具开发、云端 IoT 适配开发
最后更新：2026-07-21

## 0. 先读这部分

设备端只需要实现 4 件事：

| 序号 | 能力 | 协议 |
| --- | --- | --- |
| 1 | 获取 MQTT 凭证 | HTTP `POST /api/device/v1/provision` |
| 2 | 上报定位和状态 | MQTT publish `v1/devices/me/telemetry` |
| 3 | 接收云端指令 | MQTT subscribe `v1/devices/me/rpc/request/+` |
| 4 | 回传指令结果 | MQTT telemetry 上报 `event_type=command_result`，必要时同时回 ThingsBoard RPC response |

设备端不连接 Kafka，不访问商户后台，不访问运营后台，也不直接感知多租户。设备只使用：

- SaaS 设备 provisioning HTTP 地址；
- provisioning 返回的 ThingsBoard MQTT broker 地址和凭证。

云端会根据 IMEI/SN 解析设备所属商户租户，设备上报中不要携带、也不要相信任何 `tenant_id`。

## 1. 接入流程

```text
设备开机
  |
  |-- 本地是否已有 MQTT 凭证？
  |       |
  |       |-- 否：调用 SaaS /api/device/v1/provision
  |       |       |
  |       |       |-- 成功：保存 mqtt_host/mqtt_port/client_id/username/password
  |       |       |-- 失败：按错误码处理，不要快速无限重试
  |       |
  |       |-- 是：直接使用本地凭证
  |
  |-- 连接 MQTT
  |
  |-- 订阅 v1/devices/me/rpc/request/+
  |
  |-- 上报 ONLINE 状态
  |
  |-- 按周期上报定位
  |
  |-- 收到指令后执行，并上报 command_result
```

默认定位频率按 2 分钟一次设计。第一阶段容量目标是 5000 台在线设备、常规 2 分钟上报一次。

## 2. 设备身份

| 字段 | 说明 |
| --- | --- |
| `imei` | 设备硬件 IMEI。 |
| `device_sn` | 平台设备 SN。V1 中 `device_sn = imei`。 |
| `model_code` | 型号编码，例如 `PET-LTE-001`。用于选择一型一密 secret。 |
| `firmware_version` | 固件版本，例如 `1.0.0`。 |
| `protocol_version` | 本协议版本，V1 建议传 `v1`。 |

设备出厂前，平台侧必须已经完成：

1. 导入设备 IMEI/SN。
2. 创建设备型号，并配置该型号的 provisioning secret。
3. 在 ThingsBoard 中创建设备和 MQTT credential。
4. 将设备分配给某个商户租户。
5. 为设备生成绑定二维码内容，交给工厂预印。

设备未导入、未分配商户、型号不匹配、被冻结或作废时，provisioning 会失败。

## 3. HTTP Provisioning

### 3.1 请求

```http
POST {SAAS_BASE_URL}/api/device/v1/provision
Content-Type: application/json
```

该接口不使用 bearer token。设备身份认证只靠请求体中的 HMAC-SHA256 签名。

请求 body：

```json
{
  "imei": "860000000000000",
  "device_sn": "860000000000000",
  "model_code": "PET-LTE-001",
  "firmware_version": "1.0.0",
  "protocol_version": "v1",
  "timestamp": "2026-07-21T10:00:00Z",
  "nonce": "nonce-0001",
  "signature": "14dfa842148c43ef285af44e7b1d64ff62fb1be5f6f4a6de750131bd0a1ad73d"
}
```

字段规则：

| 字段 | 必填 | 规则 |
| --- | --- | --- |
| `imei` | 条件必填 | 与 `device_sn` 至少填一个；V1 建议两个都填相同值。 |
| `device_sn` | 条件必填 | 与 `imei` 至少填一个；V1 等于 IMEI。 |
| `model_code` | 是 | 必须和平台导入的设备型号一致。 |
| `firmware_version` | 否 | 成功后云端会记录该版本。 |
| `protocol_version` | 否 | V1 建议传 `v1`。 |
| `timestamp` | 是 | RFC3339/RFC3339Nano；推荐 UTC `Z`。默认允许 5 分钟时间偏差。 |
| `nonce` | 是 | 每次请求生成新的随机字符串。同一设备短时间内不能复用。 |
| `signature` | 是 | HMAC-SHA256，支持 hex、base64、无 padding base64。推荐 lowercase hex。 |

### 3.2 签名

签名前，设备端对参与签名的字段做 trim，然后按固定顺序拼接。可选字段为空时也必须保留该行。

Canonical string：

```text
imei={imei}
device_sn={device_sn}
model_code={model_code}
firmware_version={firmware_version}
protocol_version={protocol_version}
timestamp={timestamp}
nonce={nonce}
```

签名算法：

```text
signature = HMAC_SHA256(canonical_string, provisioning_secret_of_model_code)
```

重要细节：

- 行分隔符固定为 `\n`。
- 最后一行 `nonce={nonce}` 后面不要额外追加换行。
- 签名使用 trim 后的原始字段字符串，不要把 timestamp 重新格式化后再签。
- `provisioning_secret` 是一型一密，同一个 `model_code` 使用同一个 secret。

测试向量：

```text
secret = device-provisioning-test-secret

canonical =
imei=860000000000000
device_sn=860000000000000
model_code=PET-LTE-001
firmware_version=1.0.0
protocol_version=v1
timestamp=2026-07-21T10:00:00Z
nonce=nonce-0001

expected lowercase hex signature =
14dfa842148c43ef285af44e7b1d64ff62fb1be5f6f4a6de750131bd0a1ad73d
```

### 3.3 成功响应

成功响应：

```json
{
  "data": {
    "mqtt_host": "mqtt.example.com",
    "mqtt_port": 8883,
    "client_id": "860000000000000",
    "username": "tb_access_token",
    "password": "",
    "credential_type": "ACCESS_TOKEN",
    "issued_at": "2026-07-21T10:00:01Z"
  },
  "request_id": "req_..."
}
```

设备端要保存 `data` 中的 MQTT 参数。

| 字段 | 用法 |
| --- | --- |
| `mqtt_host` | MQTT broker host。 |
| `mqtt_port` | MQTT broker port。开发环境常见 `1883`，生产应使用 TLS 端口。 |
| `client_id` | MQTT client id。V1 返回 `device_sn`。 |
| `username` | ThingsBoard `ACCESS_TOKEN` credential。 |
| `password` | V1 为空字符串。 |
| `credential_type` | V1 固定为 `ACCESS_TOKEN`。 |
| `issued_at` | 凭证返回时间。 |

建议策略：

- 设备重启后优先使用本地保存的 MQTT 参数连接。
- 本地无凭证、恢复出厂设置、云端轮换凭证或 MQTT 长期认证失败时，再重新 provisioning。
- 重新 provisioning 必须使用新的 `nonce`。

### 3.4 错误响应

错误响应：

```json
{
  "error": {
    "code": "INVALID_DEVICE_SIGNATURE",
    "message": "Device provisioning signature is invalid"
  },
  "request_id": "req_..."
}
```

设备端按 HTTP status 和 `error.code` 处理：

| HTTP | `error.code` | 设备端处理 |
| --- | --- | --- |
| 400 | `VALIDATION_ERROR` | 检查必填字段、timestamp 格式、timestamp 偏差。 |
| 401 | `INVALID_DEVICE_SIGNATURE` | 检查 secret、canonical string、签名编码。不要快速无限重试。 |
| 404 | `DEVICE_NOT_FOUND` | IMEI/SN 未导入平台，进入产测/售后异常流程。 |
| 409 | `DEVICE_NONCE_REPLAY` | nonce 重复，生成新 nonce 后重试。 |
| 409 | `DEVICE_MODEL_MISMATCH` | 设备型号和平台导入型号不一致。 |
| 409 | `DEVICE_NOT_IN_TENANT` | 设备尚未分配商户，不能接入。 |
| 409 | `DEVICE_FROZEN` | 设备被冻结，停止正常业务上报。 |
| 409 | `DEVICE_VOIDED` | 设备作废，停止正常业务上报。 |
| 409 | `INVALID_DEVICE_STATUS` | 设备生命周期或内部 IoT credential 不满足接入条件。 |

## 4. MQTT 连接

设备使用 provisioning 返回的 MQTT 参数连接 ThingsBoard MQTT broker。

| 参数 | 值 |
| --- | --- |
| Host | `data.mqtt_host` |
| Port | `data.mqtt_port` |
| Client ID | `data.client_id` |
| Username | `data.username` |
| Password | `data.password`，V1 为空 |
| Keepalive | 建议 60 秒 |
| QoS | 上报建议 QoS 1 |
| TLS | 生产必须启用 |

V1 使用 ThingsBoard MQTT Device API：

| 用途 | Topic |
| --- | --- |
| 上报 telemetry | `v1/devices/me/telemetry` |
| 订阅云端 RPC | `v1/devices/me/rpc/request/+` |
| 回复 ThingsBoard RPC | `v1/devices/me/rpc/response/{requestId}` |

连接成功后，设备应立即：

1. 订阅 `v1/devices/me/rpc/request/+`。
2. 上报一条 `event_type=state` 的 `ONLINE` 状态。
3. 开始常规定位上报。

断线后：

- 指数退避重连。
- 重连成功后重新订阅 RPC topic。
- 补传离线期间的定位点。

## 5. MQTT 上报格式

设备所有业务事件都 publish 到：

```text
v1/devices/me/telemetry
```

payload 使用 JSON。V1 通过 `event_type` 区分事件：

| `event_type` | 含义 |
| --- | --- |
| `location` | 定位点 |
| `state` | 设备在线/电量/固件/网络状态 |
| `command_result` | 指令执行结果 |

### 5.1 定位事件

设备 publish：

```json
{
  "event_type": "location",
  "message_id": "loc_860000000000000_20260721T100000Z_000001",
  "imei": "860000000000000",
  "device_sn": "860000000000000",
  "event_time": "2026-07-21T10:00:00Z",
  "longitude": 121.4737,
  "latitude": 31.2304,
  "coordinate_system": "WGS84",
  "accuracy": 12,
  "source": "GPS",
  "battery_level": 82,
  "network_type": "LTE",
  "signal_strength": -72,
  "speed": 1.4,
  "heading": 128,
  "altitude": 8.5,
  "boot_id": "boot_20260721_001",
  "sequence_no": 1024,
  "is_offline_upload": false
}
```

字段规则：

| 字段 | 必填 | 规则 |
| --- | --- | --- |
| `event_type` | 是 | 固定 `location`。 |
| `imei` / `device_sn` | 是 | 至少一个；V1 建议两个都传。 |
| `event_time` | 是 | 定位产生时间，不是补传时间。 |
| `longitude` | 是 | 经度，范围 `[-180, 180]`。 |
| `latitude` | 是 | 纬度，范围 `[-90, 90]`。 |
| `coordinate_system` | 否 | 推荐 `WGS84`；若不是 WGS84 必须如实填写。 |
| `accuracy` | 否 | 定位精度，单位米。 |
| `source` | 否 | 建议 `GPS`、`LBS`、`WIFI`、`MIXED`。 |
| `battery_level` | 否 | 电量百分比整数。 |
| `network_type` | 否 | 网络类型，例如 `LTE`。 |
| `signal_strength` | 否 | 信号强度，建议 dBm。 |
| `speed` | 否 | 速度，建议 m/s。 |
| `heading` | 否 | 航向角，建议 `[0, 360)`。 |
| `altitude` | 否 | 海拔，单位米。 |
| `boot_id` | 建议 | 当前启动周期 ID。 |
| `sequence_no` | 建议 | 当前启动周期内递增序号。 |
| `is_offline_upload` | 是 | 实时点填 `false`，离线补传填 `true`。 |
| `message_id` | 强烈建议 | 幂等 ID，同一事件重传时必须保持不变。 |

云端处理要点：

- 重复 `message_id` 不应重复写入 latest。
- 离线补传的旧点可进入轨迹，但不得覆盖最新位置。
- 云端根据 IMEI/SN 解析租户，不使用设备上报的租户信息。

### 5.2 状态事件

设备 publish：

```json
{
  "event_type": "state",
  "message_id": "state_860000000000000_20260721T100000Z",
  "imei": "860000000000000",
  "device_sn": "860000000000000",
  "online_status": "ONLINE",
  "event_time": "2026-07-21T10:00:00Z",
  "battery_level": 82,
  "firmware_version": "1.0.1",
  "network_type": "LTE",
  "signal_strength": -72
}
```

字段规则：

| 字段 | 必填 | 规则 |
| --- | --- | --- |
| `event_type` | 是 | 固定 `state`。 |
| `imei` / `device_sn` | 是 | 至少一个；V1 建议两个都传。 |
| `online_status` | 是 | `ONLINE`、`OFFLINE` 或 `STALE`。 |
| `event_time` | 是 | 状态产生时间。 |
| `battery_level` | 否 | 电量百分比整数。 |
| `firmware_version` | 否 | 固件版本。 |
| `network_type` | 否 | 网络类型。 |
| `signal_strength` | 否 | 信号强度，建议 dBm。 |
| `message_id` | 建议 | 幂等 ID。 |

建议：

- MQTT 连接成功后立即上报 `ONLINE`。
- 正常心跳可和定位事件合并，也可单独每 2 分钟上报一次。
- 设备主动关机前可上报 `OFFLINE`。
- 设备异常断电时无法上报 `OFFLINE`，云端会通过连接状态或超时策略判断离线/过期。

### 5.3 命令结果事件

设备 publish：

```json
{
  "event_type": "command_result",
  "message_id": "cmd_result_cmd_uuid_001",
  "command_id": "cmd_uuid",
  "status": "ACKNOWLEDGED",
  "event_time": "2026-07-21T10:00:12Z"
}
```

失败示例：

```json
{
  "event_type": "command_result",
  "message_id": "cmd_result_cmd_uuid_002",
  "command_id": "cmd_uuid",
  "status": "FAILED",
  "event_time": "2026-07-21T10:00:12Z",
  "failure_code": "UNSUPPORTED_PARAMETER",
  "failed_reason": "duration_seconds exceeds device limit"
}
```

字段规则：

| 字段 | 必填 | 规则 |
| --- | --- | --- |
| `event_type` | 是 | 固定 `command_result`。 |
| `command_id` | 是 | 云端下发指令中的 SaaS 指令 ID。 |
| `status` | 是 | `ACKNOWLEDGED` 或 `FAILED`。 |
| `event_time` | 建议 | 执行结果发生时间；为空时云端使用接收时间。 |
| `failure_code` | 失败时建议 | 稳定错误码，大写蛇形，例如 `UNSUPPORTED_PARAMETER`。 |
| `failed_reason` | 否 | 简短原因，不得包含敏感信息。 |
| `message_id` | 建议 | 幂等 ID。 |

兼容状态别名：

- `ACK`、`ACKED`、`ACKNOWLEDGED` 都会归一为 `ACKNOWLEDGED`。
- `FAILED`、`FAILURE`、`ERROR` 都会归一为 `FAILED`。

云端首个终态结果生效。重复或晚到的终态结果不会覆盖已有状态。

## 6. 云端指令

设备必须订阅：

```text
v1/devices/me/rpc/request/+
```

云端下发 RPC payload：

```json
{
  "method": "SOUND",
  "params": {
    "command_id": "cmd_uuid",
    "duration_seconds": 10
  }
}
```

字段规则：

| 字段 | 说明 |
| --- | --- |
| `method` | 指令类型。 |
| `params.command_id` | SaaS 指令 ID。设备回传 `command_result` 时必须原样带回。 |
| `params.*` | 指令参数。设备应忽略未知参数。 |

V1 指令类型：

| `method` | 含义 | 示例参数 |
| --- | --- | --- |
| `HIGH_FREQUENCY_LOCATION_START` | 开启高频定位 | `{ "interval_seconds": 10, "duration_seconds": 300 }` |
| `HIGH_FREQUENCY_LOCATION_STOP` | 关闭高频定位 | `{}` |
| `SOUND` | 蜂鸣/响铃 | `{ "duration_seconds": 10 }` |
| `LIGHT` | 灯光提示 | `{ "duration_seconds": 10, "pattern": "blink" }` |
| `LOCATION_FREQUENCY` | 调整常规定位频率 | `{ "interval_seconds": 120 }` |
| `SHUTDOWN` | 关机 | `{}` |

设备处理规则：

1. 收到 RPC 后，从 topic 中取出 ThingsBoard `requestId`。
2. 校验 `params.command_id` 是否存在。
3. 校验 `method` 和参数是否支持。
4. 执行动作。
5. publish `event_type=command_result`。
6. 如需响应 ThingsBoard RPC，publish 到 `v1/devices/me/rpc/response/{requestId}`。

ThingsBoard RPC response 示例：

```json
{
  "success": true,
  "command_id": "cmd_uuid"
}
```

注意：业务指令状态以 `command_result` 为准。RPC response 只表示 ThingsBoard 层请求闭环，不替代业务结果上报。

当前云端实现注意点：后端下发 ThingsBoard RPC 时必须把 SaaS `command_id` 放入 `params.command_id`。如果联调时设备收到的 RPC 没有 `command_id`，这是云端适配问题，需要先修云端，否则设备无法正确回传业务指令结果。

## 7. 离线补传

设备断网时应缓存定位点。恢复连接后补传：

- 保留原始 `event_time`。
- `is_offline_upload=true`。
- `message_id` 保持原值，不能因为重传而变化。
- `boot_id` 和 `sequence_no` 保持原值。
- 按原始采集顺序补传。
- 建议限速，第一版建议最多 5 条/秒，避免长时间离线后瞬时冲击 broker。

旧定位点可以进入轨迹，但不会覆盖云端最新位置。

## 8. 幂等要求

设备端必须假设 MQTT QoS 1、断线重连、云端 worker 重启都会造成重复投递。

推荐生成：

| 事件 | 幂等字段 |
| --- | --- |
| 定位 | `message_id`，并同时带 `boot_id + sequence_no` |
| 状态 | `message_id` |
| 命令结果 | `message_id`，并带 `command_id` |

`message_id` 生成建议：

```text
location: loc_{imei}_{boot_id}_{sequence_no}
state:    state_{imei}_{event_time}
command:  cmd_result_{command_id}_{status}
```

同一事件重传时，`message_id` 必须完全相同。

## 9. 安全要求

- 生产 MQTT 必须使用 TLS。
- provisioning secret、MQTT username/password、signature、QR 原文不得写入普通日志、串口日志或异常上报。
- 设备本地保存 MQTT credential 时，应使用安全存储；至少要限制明文文件读取权限。
- `failed_reason`、状态上报、诊断上报不得包含用户手机号、家庭地址、Wi-Fi 密码、token 等敏感信息。
- 多次签名失败或认证失败后，应指数退避，避免快速重试。

## 10. 上报频率建议

| 场景 | 建议 |
| --- | --- |
| 常规定位 | 120 秒一次 |
| 高频定位 | 5-15 秒一次，由云端命令控制持续时间 |
| 状态心跳 | 120 秒一次，或和定位合并 |
| 低电量变化 | 阈值触发时立即上报状态 |
| 离线补传 | 恢复连接后限速补传 |

## 11. 云端内部适配参考

本节给云端 IoT 适配开发看，设备端不需要实现。

ThingsBoard Rule Chain / Adapter 需要把设备 telemetry 转为 SaaS worker 消费的 Kafka 事件：

| 设备 `event_type` | Kafka topic | Go DTO |
| --- | --- | --- |
| `location` | `location.normalized` | `iotevent.LocationEvent` |
| `state` | `device.state` | `iotevent.DeviceStatusEvent` |
| `command_result` | `command.result` | `iotevent.CommandResultEvent` |

归一化规则：

- `event_type=location` 去掉 `event_type` 后映射为 `location.normalized`。
- `event_type=state` 映射为 `device.state`，`online_status` 必须是 `ONLINE`、`OFFLINE` 或 `STALE`。
- `event_type=command_result` 映射为 `command.result`。
- `received_time` 可以由适配层补充为云端接收时间。
- `tenant_id`、`device_id` 必须由云端根据 IMEI/SN 查询得出，不能使用设备上报值。

## 12. 联调验收清单

设备端和云端至少跑通：

1. 正确签名 provisioning 成功，返回 MQTT 参数。
2. 错误 secret 返回 `INVALID_DEVICE_SIGNATURE`。
3. 重复 nonce 返回 `DEVICE_NONCE_REPLAY`。
4. 未导入 IMEI 返回 `DEVICE_NOT_FOUND`。
5. 未分配商户返回 `DEVICE_NOT_IN_TENANT`。
6. MQTT 使用返回的 username/password 连接成功。
7. 连接成功后设备上报 `event_type=state`、`online_status=ONLINE`。
8. 常规定位上报后，App/后台能看到最新位置。
9. 离线补传旧点后，轨迹有历史点，但最新位置不回滚。
10. 重复发送同一定位 `message_id`，云端不会重复更新 latest。
11. 云端下发 `SOUND`，设备收到 RPC。
12. 设备回传 `command_result=ACKNOWLEDGED` 后，云端指令状态更新。
13. 不支持的指令或参数回 `FAILED` 和稳定 `failure_code`。
14. MQTT 断线重连后，设备重新订阅 RPC 并继续上报。

## 13. 待冻结问题

这些问题需要在生产联调前确认：

- 生产 MQTT 域名、TLS 端口、CA 证书和证书 pinning 策略。
- ThingsBoard Rule Chain / Adapter 的实际部署方式。
- 云端 RPC 下发是否已保证 `params.command_id` 一定存在。
- 是否需要区分“已收到指令”和“动作执行完成”。V1 目前只有 `ACKNOWLEDGED` 和 `FAILED`。
- 国内地图链路使用 WGS84 还是 GCJ02。如果设备直接上报 GCJ02，必须明确填写 `coordinate_system=GCJ02`。
- OTA、配置下发、credential 轮换、低电量/拆卸告警是否纳入 V1。

## 14. 参考文件

- SaaS provisioning API：`backend/openapi/phase20.yaml`
- SaaS API contract：`docs/development/API_CONTRACT.md`
- SaaS data model：`docs/development/DATA_MODEL.md`
- ThingsBoard MQTT telemetry：`v1/devices/me/telemetry`
- ThingsBoard MQTT server-side RPC：`v1/devices/me/rpc/request/+`
