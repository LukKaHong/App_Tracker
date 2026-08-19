# 宠物定位器硬件指令协议V1.1

设备只需完成两件事：订阅 RPC 指令，执行后通过 telemetry 回写结果。

## 1. Topic 与报文

订阅（QoS 1）：

```text
v1/devices/me/rpc/request/+
```

回写（QoS 1）：

```text
v1/devices/me/telemetry
```

下行示例：

```json
{
  "method": "SOUND",
  "params": {
    "command_id": "cmd_uuid",
    "duration_seconds": 30
  }
}
```

解析要求：

- `method`：大写字符串，取值见第 2 节。
- `params`：JSON object。
- `params.command_id`：必填非空字符串，用于去重和回写，必须原样保留。
- 未知参数忽略；显式参数优先于默认值。
- JSON、`params` 或 `command_id` 无效时不执行动作。
- Topic 中的 `requestId` 不是业务 ID，不能代替 `command_id`。

## 2. 指令处理

| `method` | 参数及默认值 | 设备处理 |
| --- | --- | --- |
| `SOUND` | `duration_seconds=30` | 启动声音后立即 ACK，到期自动关闭。 |
| `LIGHT` | `duration_seconds=30` | 启动灯效后立即 ACK，到期自动关闭。 |
| `DEVICE_MODE` | mode | 设备收到后立即ACK |

 `duration_seconds` 必须是设备支持范围内的正整数，否则回写
`FAILED/INVALID_PARAMETER`。

设备模式：

| `mode` | 说明 |
| --- | --- |
| `searching` | 寻宠模式 |
| `walking` | 遛宠模式 |
| `supervise` | 看护模式 |
| `lowpower` | 省电模式 |
| `sleep` | 休眠模式 |



声音、灯效共同规则：

- 不同 `command_id` 的新指令重新执行并重新计时。
- 默认 30 秒自然结束时不发送第二条 `command_result`，也不额外上报关闭状态。

## 3. 结果回写

成功：

```json
{
  "event_type": "command_result",
  "message_id": "cmd_result_cmd_uuid_ACKNOWLEDGED",
  "command_id": "cmd_uuid",
  "status": "ACKNOWLEDGED",
  "event_time": "2026-08-12T10:00:12Z"
}
```

失败：

```json
{
  "event_type": "command_result",
  "message_id": "cmd_result_cmd_uuid_FAILED",
  "command_id": "cmd_uuid",
  "status": "FAILED",
  "event_time": "2026-08-12T10:00:12Z",
  "failure_code": "ACTUATOR_FAULT",
  "failed_reason": "speaker start failed"
}
```

| 字段 | 要求 |
| --- | --- |
| `event_type` | 必填，固定为 `command_result`。 |
| `message_id` | 必填；同一结果重传时保持不变。 |
| `command_id` | 必填；原样使用下行值。 |
| `status` | 必填；只允许 `ACKNOWLEDGED` 或 `FAILED`。 |
| `event_time` | 必填；UTC RFC3339。 |
| `failure_code` | `FAILED` 时必填；使用稳定的大写蛇形错误码。 |
| `failed_reason` | 可选；简短诊断，不得包含 credential 或用户隐私。 |

`ACKNOWLEDGED` 表示动作已经成功启动或配置已经生效，不表示 30 秒动作已经结束。

## 4. 幂等与断网

- 按 `command_id` 去重。
- 相同 `command_id` 重复到达时不重复执行，只重发之前缓存的相同结果。
- 建议保存最近 50 条或 24 小时的终态结果。
- 结果发送前先持久化；未收到 PUBACK 时保留在 outbox，重连后继续发送。
- 重传时 `message_id`、`command_id`、`status` 和 `failure_code` 保持不变。
- MQTT 重连后重新订阅 RPC Topic。

## 5. 失败码

| `failure_code` | 含义 |
| --- | --- |
| `UNSUPPORTED_COMMAND` | 不支持 `method`。 |
| `INVALID_PARAMETER` | 参数缺失、类型错误或超出范围。 |
| `UNSUPPORTED_PARAMETER` | 不支持参数值或灯效 pattern。 |
| `DEVICE_BUSY` | 设备处于互斥状态。 |
| `LOW_BATTERY` | 电量策略禁止执行。 |
| `GPS_UNAVAILABLE` | 无法启动高频定位。 |
| `ACTUATOR_FAULT` | 蜂鸣器、灯或驱动异常。 |
| `STORAGE_ERROR` | 去重记录或结果无法持久化。 |
| `SHUTDOWN_REJECTED` | 当前状态不允许关机。 |
| `INTERNAL_ERROR` | 其他固件内部错误。 |
