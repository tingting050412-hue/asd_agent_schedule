# ASD Schedule Storage — 面向 ASD 儿童的日程提醒模块

面向自闭症儿童（ASD）的桌面智能体日程提醒模块。

本项目基于 ESP-IDF（C 语言）开发，运行平台为 **ESP32-P4-Function-EV-Board**。  
当前模块负责日程配置持久化、后台时间匹配和触发事件通知，后续可继续对接 LVGL UI、AI 对话和 TTS 播报模块。

## 功能概述

已实现：

- 使用 NVS 持久化保存日程配置，最多 20 条
- 日程任务类型固定（6 种可选），家长不能自定义任务名称，但同一种任务可在不同时间多次使用，NVS 只保存 `task_id`，不保存字符串
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

## 固定任务 ID（共 20 种）

家长可通过 UI 对每条日程独立设置**是否启用**和**提醒时间**，但任务名称固定不可自定义。

| task_id | 宏名 | 任务名称 | 默认时间 | 默认状态 | 干预类别 |
|---------|------|----------|----------|----------|----------|
| 10 | `TASK_MORNING_ROUTINE`  | 晨起洗漱常规       | 07:00 | 开启 | 生活自理 |
| 11 | `TASK_DRESSING`         | 穿衣动作拆解       | 07:15 | 开启 | 生活自理 |
| 12 | `TASK_TOILET_AM`        | 如厕引导（上午）   | 10:00 | 开启 | 生活自理 |
| 13 | `TASK_LUNCH`            | 午餐用餐常规       | 12:00 | 开启 | 生活自理 |
| 14 | `TASK_TOILET_PM`        | 如厕引导（下午）   | 15:30 | 开启 | 生活自理 |
| 15 | `TASK_HYDRATION`        | 补充水分和零食     | 16:00 | 开启 | 生活自理 |
| 16 | `TASK_BEDTIME_PREP`     | 睡前准备常规       | 20:00 | 开启 | 生活自理 |
| 17 | `TASK_SLEEP_RITUAL`     | 关灯睡觉仪式       | 20:30 | 开启 | 生活自理 |
| 20 | `TASK_LUNCH_TRANSITION` | 午餐前5分钟过渡    | 11:55 | 开启 | 过渡缓冲 |
| 21 | `TASK_AFTERNOON_HUG`    | 下午放松与拥抱     | 14:00 | 开启 | 过渡缓冲 |
| 22 | `TASK_ACTIVITY_END`     | 娱乐结束倒计时     | 16:55 | 开启 | 过渡缓冲 |
| 30 | `TASK_AI_CHAT`          | AI谈心             | 16:30 | 开启 | 社交语言 |
| 31 | `TASK_SHARE_MOMENT`     | 分享小行为时间     | 19:00 | 开启 | 社交语言 |
| 32 | `TASK_AI_SOCIAL`        | AI模拟社交对对碰   | 19:15 | 开启 | 社交语言 |
| 40 | `TASK_TOY_CLEANUP`      | 玩具回自己的家     | 17:00 | 开启 | 认知专注 |
| 41 | `TASK_READING`          | 绘本时间           | 18:30 | 开启 | 认知专注 |
| 50 | `TASK_BREATHING`        | 深呼吸时间         | 15:00 | 开启 | 感觉统合 |
| 51 | `TASK_SENSORY`          | 感觉统合转换练习   | 10:30 | **关闭** | 感觉统合 |
| 60 | `TASK_MED_MORNING`      | 早晨用药           | 08:00 | **关闭** | 用药提醒 |
| 61 | `TASK_MED_EVENING`      | 晚间用药           | 19:30 | **关闭** | 用药提醒 |

> `task_id 51 / 60 / 61` 默认关闭，需家长在 UI 中手动启用。

## 默认日程（首次启动自动写入，共 20 条）

```text
index  0 — 07:00  晨起洗漱常规       (id=10) ✓
index  1 — 07:15  穿衣动作拆解       (id=11) ✓
index  2 — 08:00  早晨用药           (id=60) ✗ 默认关闭
index  3 — 10:00  如厕引导（上午）   (id=12) ✓
index  4 — 10:30  感觉统合转换练习   (id=51) ✗ 默认关闭
index  5 — 11:55  午餐前5分钟过渡    (id=20) ✓
index  6 — 12:00  午餐用餐常规       (id=13) ✓
index  7 — 14:00  下午放松与拥抱     (id=21) ✓
index  8 — 15:00  深呼吸时间         (id=50) ✓
index  9 — 15:30  如厕引导（下午）   (id=14) ✓
index 10 — 16:00  补充水分和零食     (id=15) ✓
index 11 — 16:30  AI谈心             (id=30) ✓
index 12 — 16:55  娱乐结束倒计时     (id=22) ✓
index 13 — 17:00  玩具回自己的家     (id=40) ✓
index 14 — 18:30  绘本时间           (id=41) ✓
index 15 — 19:00  分享小行为时间     (id=31) ✓
index 16 — 19:15  AI模拟社交对对碰   (id=32) ✓
index 17 — 19:30  晚间用药           (id=61) ✗ 默认关闭
index 18 — 20:00  睡前准备常规       (id=16) ✓
index 19 — 20:30  关灯睡觉仪式       (id=17) ✓
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

- 调用 `time_sync_get_now()` 获取真实北京时间
- SNTP 尚未同步时打印 `Waiting for time sync...` 并跳过本轮，不触发任何日程
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

### 2. 无网络时的行为

Wi-Fi 未连接或 SNTP 尚未同步时，monitor 每秒打印：

```text
Waiting for time sync...
```

不会触发任何日程，等待联网校时完成后自动恢复正常。

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
