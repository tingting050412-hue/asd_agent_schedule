# Architecture

本文档说明日程提醒模块与 UI、AI/TTS 模块之间的关系。

## Project Role

本仓库当前聚焦“日程提醒模块”，面向 ESP32-P4-Function-EV-Board 和 ESP-IDF 环境。

当前模块边界：

- `schedule_storage`：负责最多 20 个日程配置的 NVS 持久化，并提供 `task_id` 到任务名称的映射。
- `schedule_monitor`：负责后台时间监测和日程匹配。
- `schedule_event`：负责把触发结果转换为事件并发送到 FreeRTOS Queue。
- `wifi_manager`：负责 Wi-Fi Station 连接，拿到 IP 后启动 SNTP。
- `time_sync`：负责 SNTP 校时和北京时间读取。
- `UI`：后续由 LVGL 模块实现，负责输入日程和显示提醒卡片。
- `AI/TTS`：后续根据日程任务生成引导语并播报。

## Current Data Flow

### Save Schedule

```text
LVGL UI
    ↓
schedule_save_from_ui()
    ↓
schedule_storage
    ↓
NVS
```

UI 不直接操作 NVS。兼容的单日程保存仍可使用 `schedule_save_from_ui()`，日程列表功能应使用 `schedule_save_by_index(index, &cfg)`。

### Trigger Schedule

```text
schedule_monitor
    ↓
schedule_get_current()
    ↓
schedule_storage
    ↓
NVS
```

`schedule_monitor` 周期性读取全部日程配置，并与当前时间逐条匹配。

NVS key 组织方式：

```text
schedule_count
sch_0
sch_1
...
sch_19
```

每个 `sch_x` 保存一个 `schedule_config_t` blob。当前结构只保存 `hour`、`minute`、`task_id`、`enabled`，不保存任务名称字符串。

### Notify UI

```text
schedule_monitor
    ↓
schedule_event_send()
    ↓
schedule_event
    ↓
FreeRTOS Queue
    ↓
LVGL UI
```

当日程命中时，`schedule_monitor` 调用 `schedule_event_send()`，事件模块生成 `schedule_event_t` 并写入 Queue。UI 模块通过 `schedule_event_get_queue()` 获取 Queue 句柄并接收提醒事件。

## Full Target Architecture

```text
              ┌──────────────────────┐
              │     wifi_manager     │
              └──────────┬───────────┘
                         │ got ip
                         ↓
              ┌──────────────────────┐
              │      time_sync       │
              └──────────┬───────────┘
                         │ real time
                         ↓
              ┌──────────────────────┐
              │   schedule_monitor   │
              └──────────┬───────────┘
                         │ triggered
                         ↓
              ┌──────────────────────┐
              │    schedule_event    │
              └──────────┬───────────┘
                         ↓
                 FreeRTOS Queue
                  ┌──────┴───────┐
                  ↓              ↓
              LVGL UI         AI/TTS

                  ┌──────────────┐
                  │   LVGL UI    │
                  └──────┬───────┘
                         │ save schedule
                         ↓
              ┌──────────────────────┐
              │   schedule_storage   │
              └──────────┬───────────┘
                         ↓
                       NVS

```

## Extension Points

- Real time: `wifi_manager` starts Wi-Fi, then `time_sync` starts SNTP after IP is acquired.
- UI card: consume `schedule_event_t` from Queue and render LVGL reminder cards.
- AI/TTS: consume the same event or receive forwarded `task_id` from UI/event layer.
