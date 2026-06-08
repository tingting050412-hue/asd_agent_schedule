# Schedule Module

本文档说明日程提醒模块的内部职责、Queue 事件机制、当前测试方法和后续真实时间扩展方案。

## Module Responsibilities

### schedule_storage

`schedule_storage` 负责日程配置的持久化存储。

主要职责：

- 初始化 NVS。
- 保存来自 UI 的日程配置。
- 读取当前保存的日程配置。
- 在没有配置时写入默认日程。

核心接口：

```c
esp_err_t schedule_storage_init(void);
esp_err_t schedule_save_from_ui(int8_t hour,
                                int8_t minute,
                                int8_t task_id,
                                const char *task_name,
                                bool enabled);
esp_err_t schedule_get_current(schedule_config_t *cfg);
esp_err_t schedule_save_default(void);
```

### schedule_monitor

`schedule_monitor` 负责后台监测日程是否到达。

当前实现：

- 创建 FreeRTOS Task。
- 使用模拟时间运行。
- 每秒读取一次当前日程。
- 当模拟时间与日程时间匹配时触发提醒。
- 使用去重逻辑避免同一分钟重复触发同一任务。

当前模拟时间起点为：

```text
19:59:50
```

默认日程为：

```text
20:00
```

因此启动后约 10 秒可以观察到触发日志。

### schedule_event

`schedule_event` 负责把日程触发结果发送给 UI。

事件结构体：

```c
typedef struct {
    int8_t task_id;
    char task_name[32];
    int8_t hour;
    int8_t minute;
} schedule_event_t;
```

主要接口：

```c
esp_err_t schedule_event_init(void);
esp_err_t schedule_event_send(const schedule_config_t *cfg);
QueueHandle_t schedule_event_get_queue(void);
```

## Queue Mechanism

当前事件层使用 FreeRTOS Queue。

```text
schedule_monitor
    ↓
schedule_event_send()
    ↓
schedule_event_t
    ↓
FreeRTOS Queue
    ↓
UI task
```

Queue 特点：

- Queue 长度为 5。
- 每个元素类型为 `schedule_event_t`。
- `schedule_event_send()` 将 `schedule_config_t` 转换为 `schedule_event_t`。
- UI 通过 `schedule_event_get_queue()` 获取 Queue 句柄。

## Current Test Method

构建：

```powershell
idf.py build
```

烧录并打开串口：

```powershell
idf.py build flash monitor
```

首次启动时，若 NVS 没有日程，程序会写入默认日程。

预期日志：

```text
Default schedule saved successfully.
Schedule monitor started.
```

等待约 10 秒后，模拟时间到达 `20:00:00`，预期日志：

```text
========== Schedule Triggered ==========
Time      : 20:00:00
Task ID   : 1
Task Name : ...
========================================
```

该日志表示监测任务已触发提醒，同时事件已通过 `schedule_event_send()` 写入 Queue。

## SNTP Extension Plan

后续建议新增独立时间模块：

```text
schedule_time.c
schedule_time.h
```

建议职责：

- 初始化 SNTP。
- 等待联网校时完成。
- 设置北京时间时区。
- 提供获取当前本地时间的接口。

建议接口：

```c
esp_err_t schedule_time_init(void);
esp_err_t schedule_time_sync_start(void);
esp_err_t schedule_time_get_beijing(struct tm *timeinfo);
```

`schedule_monitor` 后续只调用时间模块获取当前时间，不直接处理 Wi-Fi、SNTP 或时区细节。

推荐初始化顺序：

```text
schedule_storage_init()
    ↓
schedule_event_init()
    ↓
Wi-Fi connected
    ↓
schedule_time_sync_start()
    ↓
schedule_monitor_start()
```

