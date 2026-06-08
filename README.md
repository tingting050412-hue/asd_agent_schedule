# ASD Schedule Storage Files

面向自闭症儿童（ASD）的桌面智能体日程提醒模块示例工程。

本项目基于 ESP-IDF 和 C 语言开发，当前运行平台为 ESP32-P4-Function-EV-Board。当前代码重点实现“日程配置持久化保存”和“后台日程触发检测”，后续可继续对接 LVGL UI、AI 对话和 TTS 播报模块。

## 功能概述

当前已实现：

- 使用 ESP-IDF NVS 保存日程配置
- 设备重启后可从 NVS 读取已保存日程
- 提供 UI 保存接口，便于后续 LVGL 回调调用
- 使用 FreeRTOS 后台任务监控日程
- 使用模拟时间从 `19:59:50` 开始每秒递增
- 当模拟时间命中保存的日程时间时，通过串口日志输出提醒

计划实现：

- SNTP 联网校时
- 使用本地 RTC 获取真实时间
- 获取北京时间
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
    char task_name[32];
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
    const char *task_name,
    bool enabled
);

esp_err_t schedule_get_current(schedule_config_t *cfg);

esp_err_t schedule_save_default(void);
```

说明：

- `schedule_storage_init()` 用于初始化 NVS
- `schedule_save_from_ui()` 是后续 LVGL 保存按钮可以调用的接口
- `schedule_get_current()` 用于读取当前保存的日程
- `schedule_save_default()` 在首次启动且没有日程时写入默认配置

### schedule_monitor

负责后台检查日程是否到达。

当前实现方式：

- 创建一个 FreeRTOS task
- 每秒读取一次 NVS 中的当前日程
- 使用模拟时间进行命中判断
- 命中后打印提醒日志
- 同一分钟内同一任务只触发一次，避免重复提醒

当前模拟时间初始值：

```c
19:59:50
```

默认日程为：

```text
20:00
```

因此烧录启动后大约等待 10 秒即可看到触发日志。

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
    PRIV_REQUIRES
        nvs_flash
    INCLUDE_DIRS
        "."
)
```

如果后续加入 SNTP 时间模块，通常还需要增加：

```cmake
esp_netif
lwip
```

如果 Wi-Fi 初始化也放在本组件中，还需要增加：

```cmake
esp_wifi
```

## 后续对接建议

### 对接 LVGL UI

LVGL 保存按钮回调中可调用：

```c
schedule_save_from_ui(hour, minute, task_id, task_name, enabled);
```

建议 UI 和存储模块之间只通过该接口交互，不直接操作 NVS。

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
                event.task_name,
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

建议新增：

```text
schedule_time.c
schedule_time.h
```

职责：

- SNTP 联网校时
- 设置北京时间时区
- 获取当前本地时间

这样 `schedule_monitor` 只关心“现在几点”，不直接处理网络和 SNTP 细节。

## 注意事项

- 当前版本使用模拟时间，不依赖联网
- 当前提醒方式是串口 `printf`，还未接入 LVGL 和 TTS
- `schedule_storage` 的对外接口应保持稳定，便于 UI 模块调用
- 如果修改默认日程后发现启动仍读取旧日程，需要先执行 `idf.py erase-flash`
- 源码中的中文注释建议统一保存为 UTF-8，避免不同终端显示乱码
