# ASD Schedule Storage — 面向 ASD 儿童的日程提醒模块

面向自闭症儿童（ASD）的桌面智能体日程提醒模块。

本项目基于 ESP-IDF（C 语言）开发，运行平台为 **ESP32-P4-Function-EV-Board**。  
当前模块负责日程配置持久化、后台时间匹配和触发事件通知，后续可继续对接 LVGL UI、AI 对话和 TTS 播报模块。

## 功能概述

已实现：

- 使用 NVS 持久化保存日程配置，最多 20 条
- 日程任务类型固定（6 种），NVS 只保存 `task_id`，不保存字符串
- `schedule_get_task_name(task_id)` 在运行时映射中文任务名称
- FreeRTOS 后台任务每秒检查日程，命中时触发一次，同分钟内不重复
- 优先使用 SNTP 北京时间（UTC+8），未联网时自动切换到模拟时间 fallback
- Wi-Fi Station 模式连接，支持 WPA2/WPA3 混合认证（SAE）
- ESP32-P4 通过板载 ESP32-C6 实现 Host Wi-Fi（`esp_hosted` + `esp_wifi_remote`）
- Wi-Fi 获取 IP 后自动启动 SNTP 校时
- 日程触发后通过 FreeRTOS Queue 向 UI 发送 `schedule_event_t` 事件

待实现（队友负责）：

- UI 收到事件后显示 LVGL 日程提醒卡片
- AI/TTS 根据 `task_id` 生成并播报引导语

## 工程结构

```text
.
├── CMakeLists.txt
├── dependencies.lock
├── managed_components/          # IDF Component Manager 托管组件（esp_hosted、esp_wifi_remote 等）
└── main/
    ├── CMakeLists.txt
    ├── idf_component.yml        # 声明 esp_hosted / esp_wifi_remote 依赖
    ├── main.c
    ├── schedule_storage.c / .h  # NVS 日程存储 + task_id 映射
    ├── schedule_monitor.c / .h  # FreeRTOS 后台时间检测
    ├── schedule_event.c  / .h   # FreeRTOS Queue 事件通知
    ├── time_sync.c       / .h   # SNTP 校时 + 北京时间获取
    ├── wifi_manager.c    / .h   # Wi-Fi Station 连接
```

## 固定任务 ID

```c
#define TASK_WAKE_UP       1   // 晨间洗漱穿衣
#define TASK_BREAKFAST     2   // 早餐礼仪
#define TASK_SOCIAL_TRAIN  3   // AI社交练习
#define TASK_BRUSH_TEETH   4   // 睡前刷牙
#define TASK_READING       5   // 阅读时间
#define TASK_SLEEP         6   // 睡觉
```

## 默认日程

首次启动（NVS 为空）时自动写入：

```text
index 0 — 07:30  晨间洗漱穿衣
index 1 — 08:00  早餐礼仪
index 2 — 16:30  AI社交练习
index 3 — 20:00  睡前刷牙
index 4 — 20:30  阅读时间
index 5 — 21:00  睡觉
```

## 模块说明

### schedule_storage

负责日程配置的持久化存储。

数据结构：

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
esp_err_t    schedule_storage_init(void);
esp_err_t    schedule_save_from_ui(int8_t hour, int8_t minute, int8_t task_id, bool enabled);
esp_err_t    schedule_save_by_index(int index, const schedule_config_t *cfg);
esp_err_t    schedule_get_by_index(int index, schedule_config_t *cfg);
esp_err_t    schedule_get_all(schedule_config_t *list, int max_num, int *out_count);
esp_err_t    schedule_delete_by_index(int index);
esp_err_t    schedule_get_count(int *count);
esp_err_t    schedule_get_current(schedule_config_t *cfg);
esp_err_t    schedule_save_default(void);
const char  *schedule_get_task_name(int8_t task_id);
```

说明：

- `schedule_save_from_ui()` 是兼容接口，固定写入 index 0；UI 多条管理请直接使用 `schedule_save_by_index()`
- `schedule_get_current()` 是兼容接口，固定读取 index 0
- NVS namespace 为 `"schedule"`，key 格式为 `schedule_count` + `sch_0` … `sch_19`
- 每条日程使用 `nvs_set_blob()` / `nvs_get_blob()` 存取，不保存字符串

### schedule_monitor

FreeRTOS 后台任务，每秒检查日程。

- 优先调用 `time_sync_get_now()` 获取真实北京时间（`REAL_TIME`）
- `time_sync_is_valid()` 返回 false 时切换为模拟时间（`MOCK_TIME`），初始值 `19:59:50`
- 遍历全部 enabled 日程，hour/minute 命中时调用 `schedule_event_send()`
- 通过 `s_last_trigger_*` 数组保证同一分钟只触发一次

### schedule_event

将日程触发结果打包为 FreeRTOS Queue 事件，供 UI 接收。

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
esp_err_t     schedule_event_init(void);
esp_err_t     schedule_event_send(const schedule_config_t *cfg);
QueueHandle_t schedule_event_get_queue(void);
```

### time_sync

SNTP 校时与北京时间获取。

```c
esp_err_t time_sync_init(void);
bool      time_sync_is_valid(void);
esp_err_t time_sync_get_now(int *hour, int *minute, int *second);
```

- 使用 `setenv("TZ", "CST-8", 1)` + `tzset()` 设置 UTC+8
- SNTP 服务器：`ntp.aliyun.com`
- `time_sync_init()` 由 `wifi_manager` 在 `IP_EVENT_STA_GOT_IP` 事件中自动调用

### wifi_manager

Wi-Fi Station 模式连接，支持 WPA2/WPA3 混合认证。

```c
esp_err_t wifi_manager_init(void);
bool      wifi_manager_is_connected(void);
```

说明：

- ESP32-P4 没有内置 Wi-Fi；通过板载 ESP32-C6 辅助芯片实现 Host Wi-Fi  
  依赖 `espressif/esp_hosted` + `espressif/esp_wifi_remote`（已在 `main/idf_component.yml` 声明）
- SSID / 密码在 `main/wifi_manager.c` 顶部配置：  
  ```c
  #define WIFI_SSID "你的WiFi名称"
  #define WIFI_PASS "你的WiFi密码"
  ```
- 最多重试 5 次；失败后 `schedule_monitor` 自动切换到模拟时间 fallback
- 认证模式通过 `CONFIG_ESP_WIFI_AUTH_*` Kconfig 宏配置，默认 `WIFI_AUTH_WPA2_PSK`

## 构建与烧录

> 使用 VS Code ESP-IDF 插件或命令行均可。

首次克隆或添加新 managed_components 后须先删除 build 目录再构建：

```powershell
idf.py fullclean
idf.py build
```

烧录并打开串口监视器：

```powershell
idf.py flash monitor
```

指定串口（例如 COM5）：

```powershell
idf.py -p COM5 flash monitor
```

退出串口监视器：`Ctrl + ]`

升级前建议擦除 flash，避免旧版 NVS 结构冲突：

```powershell
idf.py erase-flash
idf.py build flash monitor
```

## 功能测试

### 1. 首次启动

NVS 为空时自动写入默认 6 条日程，预期日志：

```text
ASD Schedule Storage + Monitor Demo Start
No schedule found. Save default ASD schedules.
Default schedules saved successfully.
Saved schedules: 6
  [0] 07:30 晨间洗漱穿衣 enabled=1
  [1] 08:00 早餐礼仪     enabled=1
  ...
Schedule monitor started.
```

### 2. 模拟时间触发测试（无 Wi-Fi）

模拟时间从 `19:59:50` 开始，约 10 秒后触发 `20:00` 日程：

```text
MOCK_TIME: 20:00:00 | Schedule count: 6

========== Schedule Triggered ==========
Source    : MOCK_TIME
Index     : 3
Time      : 20:00:00
Task ID   : 4
Task Name : 睡前刷牙
========================================
```

### 3. Wi-Fi + SNTP 真实时间测试

配置正确 SSID/密码烧录后，预期日志：

```text
I (xxx) wifi_manager: got ip: 192.168.x.x
SNTP started. Waiting for system time to become valid.
...
REAL_TIME: HH:MM:SS | Schedule count: 6
```

到达日程时间后触发：

```text
========== Schedule Triggered ==========
Source    : REAL_TIME
...
========================================
```

### 4. 断电保存验证

复位后若 NVS 有数据，日志应显示：

```text
Schedules loaded from NVS successfully.
```

## 对接建议（供队友参考）

### LVGL UI

UI 保存按钮回调：

```c
// 新增或覆盖单条日程（固定写 index 0）
schedule_save_from_ui(hour, minute, task_id, enabled);

// 多条日程管理
schedule_save_by_index(index, &cfg);
schedule_get_all(list, MAX_SCHEDULE_NUM, &count);
schedule_delete_by_index(index);
```

接收日程触发事件（在 UI 任务中）：

```c
#include "schedule_event.h"

void ui_schedule_task(void *arg)
{
    QueueHandle_t queue = schedule_event_get_queue();
    schedule_event_t event;
    while (1) {
        if (xQueueReceive(queue, &event, portMAX_DELAY) == pdTRUE) {
            // event.task_id / event.hour / event.minute
            const char *name = schedule_get_task_name(event.task_id);
            // 根据 task_id 显示对应图标、动画、卡片
        }
    }
}
```

### AI / TTS

触发时只需向 AI/TTS 传递 `task_id`，由 AI 根据 `task_id` 生成适合 ASD 儿童的引导语：

```text
task_id → AI/TTS 生成语音引导语 → 播报
```

## 注意事项

- 从旧版（含 `task_name` 字段的 NVS 结构）升级时，需先执行 `idf.py erase-flash`
- 手机热点请开启 **2.4 GHz** 频段，ESP32 不支持 5 GHz
- 源码中的中文注释请保存为 UTF-8 编码
- `managed_components/` 目录和 `dependencies.lock` 已包含在仓库中，克隆后无需重新下载组件
