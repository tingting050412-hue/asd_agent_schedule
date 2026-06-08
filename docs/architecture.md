# Architecture

本文档说明日程提醒模块与 UI、AI/TTS 模块之间的关系。

## Project Role

本仓库当前聚焦“日程提醒模块”，面向 ESP32-P4-Function-EV-Board 和 ESP-IDF 环境。

当前模块边界：

- `schedule_storage`：负责日程配置的 NVS 持久化。
- `schedule_monitor`：负责后台时间监测和日程匹配。
- `schedule_event`：负责把触发结果转换为事件并发送到 FreeRTOS Queue。
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

UI 不直接操作 NVS，而是通过 `schedule_save_from_ui()` 保存结构化日程配置。

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

`schedule_monitor` 周期性读取当前日程配置，并与当前时间进行匹配。

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
```

## Extension Points

- Real time: replace mock time in `schedule_monitor` with SNTP and RTC based time.
- UI card: consume `schedule_event_t` from Queue and render LVGL reminder cards.
- AI/TTS: consume the same event or receive forwarded `task_id` from UI/event layer.

