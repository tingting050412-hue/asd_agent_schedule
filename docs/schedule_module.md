# Schedule Module

本文档说明日程提醒模块的内部职责、Queue 事件机制、当前测试方法和后续真实时间扩展方案。

## Module Responsibilities

### schedule_storage

`schedule_storage` 负责最多 20 个日程配置的持久化存储。系统任务类型固定，NVS 只保存 `task_id`，任务名称由 `schedule_get_task_name(task_id)` 映射得到。

主要职责：

- 初始化 NVS。
- 保存来自 UI 的日程配置。
- 按 index 读取、保存、删除日程。
- 读取当前全部日程配置。
- 在没有配置时写入 6 个默认 ASD 推荐日程。

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
esp_err_t schedule_save_by_index(int index, const schedule_config_t *cfg);
esp_err_t schedule_get_by_index(int index, schedule_config_t *cfg);
esp_err_t schedule_get_all(schedule_config_t *list, int max_num, int *out_count);
esp_err_t schedule_delete_by_index(int index);
esp_err_t schedule_get_count(int *count);
const char *schedule_get_task_name(int8_t task_id);
```

兼容说明：

- `schedule_save_from_ui()` 默认保存到 index 0。
- `schedule_get_current()` 默认读取 index 0。
- 新 UI 日程列表应使用 `schedule_save_by_index()` 和 `schedule_get_all()`。

NVS key：

```text
schedule_count
sch_0
sch_1
...
sch_19
```

每个 `sch_x` 使用 blob 保存一个 `schedule_config_t`。当前结构只包含 `hour`、`minute`、`task_id`、`enabled`。

### schedule_monitor

`schedule_monitor` 负责后台监测日程是否到达。

当前实现：

- 创建 FreeRTOS Task。
- 使用模拟时间运行。
- 每秒读取一次全部日程。
- 遍历最多 20 个日程。
- 当当前时间与任意启用日程匹配时触发提醒。
- 使用按日程 index 记录的去重逻辑，避免同一分钟重复触发同一任务。

当前模拟时间起点为：

```text
19:59:50
```

默认 6 个日程为：

```text
07:30 晨间洗漱穿衣
08:00 早餐礼仪
16:30 AI社交练习
20:00 睡前刷牙
20:30 阅读时间
21:00 睡觉
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

## Test 20 Schedules

可以在临时测试代码或 UI 测试入口中写入 index `0~19`：

```c
for (int i = 0; i < MAX_SCHEDULE_NUM; i++) {
    schedule_config_t cfg = {
        .hour = i % 24,
        .minute = i % 60,
        .task_id = TASK_READING,
        .enabled = 1,
    };
    schedule_save_by_index(i, &cfg);
}
```

然后重启设备，调用 `schedule_get_all()` 确认 `out_count` 为 20，且每条日程仍可读取。

从旧版单日程或带 `task_name` 的 NVS 结构升级时，需要执行：

```powershell
idf.py erase-flash
idf.py build flash monitor
```

这样可以清除旧 key，并写入新的默认 6 条日程。

## SNTP Extension Plan

当前已新增独立时间模块：

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

当前初始化顺序：

```text
schedule_storage_init()
    ↓
schedule_event_init()
    ↓
wifi_manager_init()
    ↓
IP_EVENT_STA_GOT_IP
    ↓
time_sync_init()
    ↓
schedule_monitor_start()
```

无 Wi-Fi 或密码错误时，`schedule_monitor` 会继续使用 `MOCK_TIME` fallback。
