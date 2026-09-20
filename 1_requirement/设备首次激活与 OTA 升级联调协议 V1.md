# 设备首次激活与 OTA 升级联调协议 V1

状态：联调稿  
读者：设备固件、通信模组和硬件测试工程师  
最后更新：2026-09-19

## 1. 首次激活

### 1.1 出厂固化数据

设备必须持久化以下出厂数据：

- `imei`：字符串。
- `device_sn`：字符串；V1 与 IMEI 相同。
- `model_code`：设备型号编码。
- `provisioning_secret`：该型号的一型一密密钥。
- `SAAS_BASE_URL`：激活接口地址前缀。

`provisioning_secret` 按原始字符串的 UTF-8 字节参与 HMAC。若密钥使用 64 位十六进制
字符串，设备使用这 64 个 ASCII 字节，不再做 hex 解码。密钥不得写入普通日志。

### 1.2 激活触发条件

设备本地没有有效 MQTT 凭证时执行激活，包括首次开机、凭证区损坏或恢复出厂设置。
发起请求前必须取得可信时间，误差不得超过 5 分钟。

```http
POST {SAAS_BASE_URL}/api/device/v1/provision
Content-Type: application/json
```

每次请求生成新的随机 `nonce`，建议使用至少 16 个随机字节并编码为 hex。

```json
{
  "imei": "860000000000008",
  "device_sn": "860000000000008",
  "model_code": "PET-LOCATOR-DEMO",
  "firmware_version": "1.0.0",
  "protocol_version": "v1",
  "timestamp": "2026-09-19T02:00:00Z",
  "nonce": "287c0a1a75876cd280417fb9afa222be",
  "signature": "lowercase_hex_hmac_sha256"
}
```

`imei` 和 `device_sn` 必须作为 JSON string 发送，不能发送 JSON number。

### 1.3 签名

签名前 trim 每个字段，按固定顺序拼接；空字段保留等号后的空值，末尾不加换行：

```text
imei={imei}
device_sn={device_sn}
model_code={model_code}
firmware_version={firmware_version}
protocol_version={protocol_version}
timestamp={timestamp}
nonce={nonce}
```

计算方式：

```text
digest = HMAC-SHA256(
  key     = provisioning_secret 的 UTF-8 字节,
  message = canonical string 的 UTF-8 字节
)
signature = digest 的小写 hex 字符串
```

### 1.4 成功响应与凭证保存

```json
{
  "data": {
    "mqtt_host": "mqtt.example.com",
    "mqtt_port": 8883,
    "client_id": "860000000000008",
    "username": "device_access_token",
    "password": "",
    "credential_type": "ACCESS_TOKEN",
    "issued_at": "2026-09-19T02:00:01Z"
  },
  "request_id": "req_123"
}
```

设备必须先校验：

- `credential_type == ACCESS_TOKEN`。
- `mqtt_host` 非空，`mqtt_port` 在 `1..65535`。
- `client_id == device_sn`。
- `username` 非空，`password` 允许为空。

校验通过后，将完整 MQTT 参数原子写入受保护的非易失存储。写入失败时不得把设备标记为
激活成功；重新生成 timestamp、nonce 和签名后再次 Provisioning。不得记录 username、
password、signature 或 provisioning secret。

### 1.5 MQTT 首次连接

使用 Provisioning 响应连接 MQTT 3.1.1：

| 参数 | 值 |
| --- | --- |
| Host / Port | `mqtt_host` / `mqtt_port` |
| Client ID | `client_id` |
| Username / Password | `username` / `password` |
| TLS | 生产环境必须启用并校验证书 |
| Keepalive | 建议 60 秒 |
| QoS | 订阅和上报使用 QoS 1 |

同一设备只能维持一个 MQTT 会话。RPC、属性、遥测和 OTA 共用该会话。连接成功并恢复全部
订阅后，上报：

```text
Topic: v1/devices/me/telemetry
```

```json
{
  "event_type": "state",
  "message_id": "state_860000000000008_boot001_1",
  "imei": "860000000000008",
  "device_sn": "860000000000008",
  "online_status": "ONLINE",
  "event_time": "2026-09-19T02:00:05Z",
  "firmware_version": "1.0.0"
}
```

MQTT 认证成功并完成 `ONLINE` 上报后，设备侧首次激活流程完成。

### 1.6 失败与重试

错误响应格式：

```json
{
  "error": {"code": "INVALID_DEVICE_SIGNATURE", "message": "..."},
  "request_id": "req_123"
}
```

| 场景 / 错误码 | 设备处理 |
| --- | --- |
| 网络超时、断网、HTTP `5xx` | 指数退避；每次重试使用新的 timestamp、nonce 和 signature。 |
| `DEVICE_NONCE_REPLAY` | 立即更换 nonce，重新签名后重试。 |
| `VALIDATION_ERROR` | 停止重试，检查固件请求字段和设备时间。 |
| `INVALID_DEVICE_SIGNATURE` | 停止重试，检查型号密钥、canonical string 和 model_code。 |
| `DEVICE_NOT_FOUND` | 停止重试，交由产测或售后处理。 |
| `DEVICE_MODEL_MISMATCH` | 停止重试，检查烧录型号。 |
| `DEVICE_NOT_IN_TENANT`、`INVALID_DEVICE_STATUS` | 停止重试，等待设备业务状态修复。 |
| `DEVICE_FROZEN`、`DEVICE_VOIDED` | 停止重试，不得继续联网。 |

即使上一次请求可能已成功但响应丢失，也必须创建一个全新的签名请求，不能原样重发旧
nonce。重复 Provisioning 返回该设备当前有效的 MQTT 凭证。

## 2. OTA 升级

### 2.1 MQTT 订阅

每次 MQTT 连接或重连后，在同一个设备会话中订阅 QoS 1：

```text
v1/devices/me/attributes
v1/devices/me/attributes/response/+
v2/fw/response/+/chunk/+
v2/fw/error
v1/devices/me/rpc/request/+        # 同时支持设备指令时订阅
```

禁止为 OTA 单独创建第二个 MQTT 连接。

### 2.2 上报当前版本并查询目标版本

上报客户端属性：

```text
Topic: v1/devices/me/attributes
Payload: {"current_fw_title":"PET-LOCATOR-DEMO","current_fw_version":"1.0.0"}
```

同时向 `v1/devices/me/telemetry` 上报同名字段。

生成十进制 `requestId`，范围固定为 `1..2147483647`。进程内递增，达到上限后回绕到
`1`；不能与仍在等待响应的请求 ID 重复，不能使用 Unix 纳秒时间戳。

```text
Topic: v1/devices/me/attributes/request/{requestId}
```

```json
{
  "sharedKeys": "fw_title,fw_version,fw_checksum,fw_checksum_algorithm,fw_size,fw_url"
}
```

响应 topic：

```text
v1/devices/me/attributes/response/{requestId}
```

响应字段可能直接位于根 object，也可能位于 `shared` object。只处理 requestId 与当前请求
一致的完整快照。收到 `v1/devices/me/attributes` 更新通知时，只把通知当作刷新信号，重新
请求完整快照；不得把多次通知中的字段拼成一个固件包。

### 2.3 元数据校验

仅在以下条件全部满足时开始升级：

| 字段 | 校验规则 |
| --- | --- |
| `fw_title` | 必须与本机 `model_code` 完全相同。 |
| `fw_version` | 非空、最长 64 字符，且不同于当前版本。 |
| `fw_checksum_algorithm` | 规范化后必须为 `SHA256`。 |
| `fw_checksum` | 64 位 SHA-256 hex。 |
| `fw_size` | 正整数，V1 不超过 16 MiB。 |
| `fw_url` | 必须缺失或为空；V1 只使用 MQTT 分片下载。 |

将 `fw_title + fw_version + fw_checksum` 作为升级任务唯一标识；同一任务不得重复安装。
元数据不完整或不合法时上报 `FAILED/INVALID_FIRMWARE_METADATA`。

### 2.4 分片下载

开始下载前上报：

```text
Topic: v1/devices/me/telemetry
Payload: {"fw_state":"DOWNLOADING","fw_error":"","current_fw_title":"PET-LOCATOR-DEMO","current_fw_version":"1.0.0"}
```

为本次下载生成新的 `requestId`，所有分片使用同一个 requestId。推荐分片大小为 4096
字节，允许范围 `1..65536`。

请求第 `N` 块：

```text
Topic:   v2/fw/request/{requestId}/chunk/{N}
Payload: 4096
```

payload 是期望字节数的十进制 ASCII，不是 JSON。响应为：

```text
Topic:   v2/fw/response/{requestId}/chunk/{N}
Payload: 原始二进制
```

处理规则：

1. 从 `N=0` 开始顺序请求，不并行请求。
2. 二进制 payload 可以包含任意字节和 `0x00`，不能当字符串处理。
3. 每收到一块立即流式更新 SHA-256，并累计字节数。
4. 普通分片长度必须等于 `min(chunk_size, fw_size - received_bytes)`。
5. 累计收到恰好 `fw_size` 字节后，再请求下一个分片；该响应必须是长度为 0 的 EOF。
6. 提前收到空包、超量非空包、乱序包或最终大小不符，升级失败。
7. 每块设置超时并有限重试；建议超时 5 秒、最多重试 3 次。
8. 断线后重新连接、重新获取完整元数据，并用新的 requestId 从第 0 块重新下载。

收到 `v2/fw/error`、分片重试耗尽或长度异常时上报 `FAILED/DOWNLOAD_FAILED`。

### 2.5 校验、安装和结果上报

状态必须按以下顺序上报到 `v1/devices/me/telemetry`：

```text
DOWNLOADING -> DOWNLOADED -> VERIFIED -> UPDATING -> UPDATED
```

处理顺序：

1. 收到严格 EOF 后，上报 `DOWNLOADED`。
2. 校验实际字节数和 SHA-256；匹配后上报 `VERIFIED`。
3. 确认电量和存储条件满足，持久化升级恢复信息，上报 `UPDATING`。
4. 写入备用分区并切换启动分区；推荐使用 A/B 分区和失败回滚。
5. 新固件启动并完成自检后，更新本地当前版本。
6. 更新客户端属性，再上报最终成功遥测：

```text
Topic: v1/devices/me/attributes
Payload: {"current_fw_title":"PET-LOCATOR-DEMO","current_fw_version":"1.1.0"}
```

```text
Topic: v1/devices/me/telemetry
Payload: {"fw_state":"UPDATED","fw_error":"","current_fw_title":"PET-LOCATOR-DEMO","current_fw_version":"1.1.0"}
```

只有新固件已经成功启动并通过自检，才能上报 `UPDATED`。下载、校验或安装失败时不得修改
当前版本：

```json
{
  "fw_state": "FAILED",
  "fw_error": "CHECKSUM_MISMATCH",
  "current_fw_title": "PET-LOCATOR-DEMO",
  "current_fw_version": "1.0.0"
}
```

设备至少支持以下稳定错误码：

| `fw_error` | 含义 |
| --- | --- |
| `INVALID_FIRMWARE_METADATA` | 元数据缺失、型号不匹配、大小越界或 URL 型包。 |
| `UNSUPPORTED_CHECKSUM_ALGORITHM` | 校验算法不是 SHA-256。 |
| `DOWNLOAD_FAILED` | 下载超时、分片异常、EOF 异常或大小不符。 |
| `CHECKSUM_MISMATCH` | SHA-256 不匹配。 |
| `INSTALL_FAILED` | 写入、切换、启动、自检或回滚失败。 |

`fw_error` 不得携带 MQTT Token、固件内容、内存地址、用户数据或其他敏感信息。同一失败
任务不得立即无限循环重试；等待新的完整属性快照或设备重启后的受控恢复流程。

## 3. 硬件联调验收项

- 使用正确型号密钥完成 Provisioning，并成功通过 MQTT 认证。
- 模拟 HTTP 响应丢失，使用新 nonce 重新 Provisioning 后仍可取得有效凭证。
- 旧 nonce 重放被拒绝，设备能生成新请求恢复。
- MQTT 重连后恢复 RPC、属性和 OTA 全部订阅，且始终只有一个设备会话。
- OTA 能处理包含 `0x00` 的任意二进制分片、最后不足整块的分片和空 EOF。
- OTA 下载中断后从第 0 块重新开始，不混用断线前后的元数据和分片。
- SHA-256 错误上报 `CHECKSUM_MISMATCH`，且当前版本不变。
- 安装失败或启动回滚上报 `INSTALL_FAILED`，且当前版本不变。
- 成功升级严格依次上报五个状态，重启自检后才上报 `UPDATED` 和新版本。
- 日志中不存在 provisioning secret、signature、MQTT Token 或固件二进制内容。
