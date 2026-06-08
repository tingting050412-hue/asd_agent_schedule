# ASD Schedule Storage Files

面向自闭症儿童（ASD）的桌面智能体日程提醒模块示例工程。

本项目基于 ESP-IDF 和 C 语言开发，当前运行平台为 ESP32-P4-Function-EV-Board。当前代码重点实现“日程配置持久化保存”和“后台日程触发检测”，后续可继续对接 LVGL UI、AI 对话和 TTS 播报模块。

## 功能概述

当前已实现：

- 使用 ESP-IDF NVS 保存日程配置
- 设备重启后可从 NVS 读取已保存日程
- 最多支持 20 个日程
- 日程任务类型固定，家长只能从预设任务中选择
- NVS 只保存 `task_id`，不保存任务名称字符串
- 提供 UI 保存接口，便于后续 LVGL 回调调用
- 使用 FreeRTOS 后台任务监控日程
- 新增 SNTP 本地时间校准模块
- 支持北京时间 UTC+8
- 有网络且 Wi-Fi 已连接时使用真实时间
- 无网络或尚未完成 SNTP 校时时，自动使用模拟时间 fallback
- 当时间命中保存的日程时间时，通过串口日志输出提醒，并发送 Queue 事件给 UI

计划实现：

- 触发日程后显示 LVGL 卡片
- 触发日程后向 AI/TTS 模块发送 `task_id`

## 工程结构

```text
.
├── CMakeLists.txt
└── main
    ├── CMakeLists.txt
    ├── main.c
    ├── schedule_storage.c
    ├── schedule_storage.h
    ├── schedule_monitor.c
    └── schedule_monitor.h
    ├── schedule_event.c
    ├── schedule_event.h
    ├── time_sync.c
    └── time_sync.h
```

## 模块说明

### schedule_storage

负责日程配置的持久化存储。

保存的数据结构：

```c
typedef struct {
    int8_t hour;
    int8_t minute;
    int8_t task_id;
    int8_t enabled;
} schedule_config_t;
```

主要接口：

```c
esp_err_t schedule_storage_init(void);

esp_err_t schedule_save_from_ui(
    int8_t hour,
    int8_t minute,
    int8_t task_id,
    bool enabled
);

esp_err_t schedule_get_current(schedule_config_t *cfg);

esp_err_t schedule_save_default(void);

esp_err_t schedule_save_by_index(int index, const schedule_config_t *cfg);

esp_err_t schedule_get_by_index(int index, schedule_config_t *cfg);

esp_err_t schedule_get_all(schedule_config_t *list, int max_num, int *out_count);

esp_err_t schedule_delete_by_index(int index);

esp_err_t schedule_get_count(int *count);

const char *schedule_get_task_name(int8_t task_id);
```

说明：

- `schedule_storage_init()` 用于初始化 NVS
- `schedule_save_from_ui()` 是兼容接口，默认写入 index 0
- `schedule_get_current()` 是兼容接口，默认读取 index 0
- `schedule_save_by_index()` 用于保存指定 index 的日程
- `schedule_get_all()` 用于读取当前全部日程，供 monitor 遍历
- `schedule_delete_by_index()` 用于删除指定 index 的日程
- `schedule_get_count()` 用于读取当前已保存日程数量
- `schedule_get_task_name()` 用于把固定 `task_id` 映射为中文任务名称
- `schedule_save_default()` 在首次启动且没有日程时写入 6 个 ASD 推荐日程

NVS key 组织方式：

```text
schedule_count
sch_0
sch_1
...
sch_19
```

每个 `sch_x` 使用 `nvs_set_blob()` / `nvs_get_blob()` 保存一个 `schedule_config_t`。结构体中只包含 `hour`、`minute`、`task_id`、`enabled`，不保存 `task_name`。

### schedule_monitor

负责后台检查日程是否到达。

当前实现方式：

- 创建一个 FreeRTOS task
- 每秒读取一次 NVS 中的全部日程
- 优先使用 `time_sync_get_now()` 获取真实北京时间
- 如果 SNTP 时间未同步，使用模拟时间进行 fallback
- 遍历最多 20 条日程并判断是否命中
- 任意日程命中后打印提醒日志
- 命中后通过 `schedule_event_send()` 发送 Queue 事件
- 每个日程同一分钟只触发一次，避免重复提醒

当前模拟时间初始值：

```c
19:59:50
```

默认日程为：

```text
07:30 晨间洗漱穿衣
08:00 早餐礼仪
16:30 AI社交练习
20:00 睡前刷牙
20:30 阅读时间
21:00 睡觉
```（我还没想好，内容再议，先暂定二十个）

无网络 fallback 测试时，模拟时间从 `19:59:50` 开始，因此启动后大约等待 10 秒即可看到 `20:00` 的默认日程触发日志。

### schedule_event

负责把日程触发结果转换为 FreeRTOS Queue 事件，供 UI 模块接收。

事件结构体：

```c
typedef struct {
    int8_t task_id;
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

### time_sync

负责 SNTP 时间同步和北京时间获取。

主要接口：

```c
esp_err_t time_sync_init(void);
bool time_sync_is_valid(void);
esp_err_t time_sync_get_now(int *hour, int *minute, int *second);
```

说明：

- `time_sync_init()` 启动 SNTP，不写死 Wi-Fi SSID 和密码
- 当前 Wi-Fi 模块尚未接入，`time_sync_init()` 暂时在 `app_main()` 中调用
- 后续队友接入 Wi-Fi 后，建议把 `time_sync_init()` 移动到 Wi-Fi connected 事件之后调用
- 使用 `setenv("TZ", "CST-8", 1)` 和 `tzset()` 设置北京时间
- 使用 `time()` 和 `localtime_r()` 获取本地时间
- 若时间尚未同步，`time_sync_get_now()` 返回 `ESP_FAIL`
- 当前 `schedule_monitor` 会在时间无效时自动使用模拟时间 fallback

## 构建与烧录

进入项目目录：

```powershell
cd c:\embedded\asd_schedule_storage_files
```

构建：

```powershell
idf.py build
```

烧录并打开串口监视器：

```powershell
idf.py build flash monitor
```

如果需要指定串口，例如 `COM5`：

```powershell
idf.py -p COM5 build flash monitor
```

退出串口监视器：

```text
Ctrl + ]
```

## 功能测试

### 1. 首次启动测试

首次烧录后，如果 NVS 中没有日程，应看到类似日志：

```text
ASD Schedule Storage + Monitor Demo Start
No schedule found. Save default schedule.
Default schedule saved successfully.
Current schedule:
  enabled   : 1
  time      : 20:00
  task_id   : 1
Schedule monitor started.
```

这表示默认日程已经保存到 NVS。

### 2. 日程触发测试

当前模拟时间从 `19:59:50` 开始运行，等待大约 10 秒后，应看到类似日志：

```text
========== Schedule Triggered ==========
Time      : 20:00:00
Task ID   : 1
Task Name : ...
========================================
```

看到该日志表示：

- NVS 读取成功
- FreeRTOS 后台任务运行正常
- 日程命中判断正常
- 提醒触发正常

### 3. 断电保存测试

按开发板复位键，或重新打开 monitor。

如果 NVS 保存成功，再次启动时应看到：

```text
Schedule loaded from NVS successfully.
```

而不是：

```text
No schedule found. Save default schedule.
```

### 4. 清空 NVS 后重新测试

如果想重新测试首次启动流程，可以擦除 flash：

```powershell
idf.py erase-flash
idf.py build flash monitor
```

## 当前 CMake 配置

`main/CMakeLists.txt` 当前需要包含：

```cmake
idf_component_register(
    SRCS
        "main.c"
        "schedule_storage.c"
        "schedule_monitor.c"
        "schedule_event.c"
        "time_sync.c"
    PRIV_REQUIRES
        nvs_flash
        esp_netif
        lwip
    INCLUDE_DIRS
        "."
)
```

如果 Wi-Fi 初始化也放在本组件中，还需要增加：

```cmake
esp_wifi
```

## 后续对接建议

### 对接 LVGL UI

LVGL 保存按钮回调中可调用：

```c
schedule_save_from_ui(hour, minute, task_id, enabled);
```

建议 UI 和存储模块之间只通过该接口交互，不直接操作 NVS。

如果 UI 后续实现日程列表，请使用：

```c
schedule_save_by_index(index, &cfg);
schedule_get_all(list, MAX_SCHEDULE_NUM, &count);
schedule_delete_by_index(index);
```

日程触发后，`schedule_monitor` 会通过 FreeRTOS Queue 发送 `schedule_event_t` 事件。UI 模块可以通过 `schedule_event_get_queue()` 获取 Queue 句柄并接收事件：

```c
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "schedule_event.h"

void ui_schedule_task(void *arg)
{
    QueueHandle_t queue = schedule_event_get_queue();
    schedule_event_t event;

    while (1) {
        if (xQueueReceive(queue, &event, portMAX_DELAY) == pdTRUE) {
            ui_show_schedule_card(
                event.task_id,
                schedule_get_task_name(event.task_id),
                event.hour,
                event.minute
            );
        }
    }
}
```

UI 模块收到事件后，可以显示 LVGL 日程提醒卡片，并根据 `task_id`、`task_name`、`hour`、`minute` 决定卡片内容和动画。

### 对接 AI/TTS

建议触发时只向 AI/TTS 模块发送结构化信息：

```text
task_id
task_name
```

AI/TTS 模块根据 `task_id` 生成适合 ASD 儿童的引导语，再进行语音播报。

### 替换真实时间

当前已新增：

```text
time_sync.c
time_sync.h
```

职责：

- SNTP 联网校时
- 设置北京时间时区
- 获取当前本地时间

当前版本不会在 `time_sync.c` 中写死 Wi-Fi 账号密码。若后续 Wi-Fi 模块由队友实现，推荐在 Wi-Fi connected 事件之后调用：

```c
time_sync_init();
```

这样 `schedule_monitor` 只关心“现在几点”，不直接处理网络和 SNTP 细节。

## SNTP 时间测试

### 无网络测试

不连接 Wi-Fi，直接烧录运行：

```powershell
idf.py build flash monitor
```

预期日志中会看到：

```text
MOCK_TIME: 19:59:xx | Schedule: 20:00 ...
```

约 10 秒后触发：

```text
Schedule Triggered
Source    : MOCK_TIME
```

### 有网络测试

当前工程还没有 Wi-Fi 模块。有网络测试需要队友的 Wi-Fi 模块先完成连接，再调用 `time_sync_init()`。

推荐后续顺序：

```text
Wi-Fi connected
    ↓
time_sync_init()
    ↓
schedule_monitor 使用 REAL_TIME
```

SNTP 同步成功后，日志应显示：

```text
REAL_TIME: HH:MM:SS | Schedule: ...
```

到达日程时间后触发：

```text
Schedule Triggered
Source    : REAL_TIME
```

## 注意事项

- 当前版本支持 SNTP 真实时间，也保留模拟时间 fallback
- 当前提醒方式保留串口 `printf`，同时已通过 Queue 向 UI 发送事件
- 当前最多支持 20 个日程，index 范围为 `0~19`
- `schedule_storage` 的对外接口应保持稳定，便于 UI 模块调用
- 从旧版单日程 NVS 结构升级到多日程结构时，建议执行 `idf.py erase-flash` 后重新烧录，以写入新的默认 6 条日程
- 源码中的中文注释建议统一保存为 UTF-8，避免不同终端显示乱码
