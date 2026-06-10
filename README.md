# ASD Schedule Storage — 面向 ASD 儿童的日程提醒模块

基于 ESP-IDF（C 语言）开发，运行平台为 **ESP32-P4-Function-EV-Board**。  
负责日程配置持久化、后台时间匹配与触发事件通知，可对接 LVGL UI、AI 对话和 TTS 播报模块。

## 功能

- NVS 持久化保存最多 20 条日程，首次启动自动写入默认配置
- 固定 20 种 ASD 干预任务，家长可通过 UI 调整每条日程的**启用状态**和**提醒时间**
- FreeRTOS 后台任务每秒检查，命中时间后触发一次事件（同分钟内不重复）
- SNTP 北京时间（UTC+8），服务器 `ntp.aliyun.com`；断网后通过 `esp_timer` 惯性走时
- Wi-Fi Station 模式，支持 WPA2/WPA3；ESP32-P4 通过板载 ESP32-C6 实现 Host Wi-Fi
- 触发事件通过 FreeRTOS Queue 推送给 UI（`schedule_event_t`）

## 工程结构

```
main/
├── main.c
├── schedule_storage.c / .h   # NVS 存储 + task_id 映射
├── schedule_monitor.c / .h   # FreeRTOS 后台时间检测
├── schedule_event.c  / .h    # Queue 事件通知
├── time_sync.c       / .h    # SNTP 校时 + 惯性走时
├── wifi_manager.c    / .h    # Wi-Fi Station 连接
└── idf_component.yml         # esp_hosted / esp_wifi_remote 依赖声明
```

## 固定任务 ID（共 20 种）

| task_id | 任务名称 | 默认时间 | 默认 | 类别 |
|---------|----------|----------|------|------|
| 10 | 晨起洗漱常规 | 07:00 | 开 | 生活自理 |
| 11 | 穿衣动作拆解 | 07:15 | 开 | 生活自理 |
| 12 | 如厕引导（上午） | 10:00 | 开 | 生活自理 |
| 13 | 午餐用餐常规 | 12:00 | 开 | 生活自理 |
| 14 | 如厕引导（下午） | 15:30 | 开 | 生活自理 |
| 15 | 补充水分和零食 | 16:00 | 开 | 生活自理 |
| 16 | 睡前准备常规 | 20:00 | 开 | 生活自理 |
| 17 | 关灯睡觉仪式 | 20:30 | 开 | 生活自理 |
| 20 | 午餐前5分钟过渡 | 11:55 | 开 | 过渡缓冲 |
| 21 | 下午放松与拥抱 | 14:00 | 开 | 过渡缓冲 |
| 22 | 娱乐结束倒计时 | 16:55 | 开 | 过渡缓冲 |
| 30 | AI谈心 | 16:30 | 开 | 社交语言 |
| 31 | 分享小行为时间 | 19:00 | 开 | 社交语言 |
| 32 | AI模拟社交对对碰 | 19:15 | 开 | 社交语言 |
| 40 | 玩具回自己的家 | 17:00 | 开 | 认知专注 |
| 41 | 绘本时间 | 18:30 | 开 | 认知专注 |
| 50 | 深呼吸时间 | 15:00 | 开 | 感觉统合 |
| 51 | 感觉统合转换练习 | 10:30 | **关** | 感觉统合 |
| 60 | 早晨用药 | 08:00 | **关** | 用药提醒 |
| 61 | 晚间用药 | 19:30 | **关** | 用药提醒 |

## 主要接口

**schedule_storage**

```c
esp_err_t    schedule_storage_init(void);
esp_err_t    schedule_save_by_index(int index, const schedule_config_t *cfg);
esp_err_t    schedule_get_by_index(int index, schedule_config_t *cfg);
esp_err_t    schedule_get_all(schedule_config_t *list, int max_num, int *out_count);
esp_err_t    schedule_delete_by_index(int index);
esp_err_t    schedule_get_count(int *count);
const char  *schedule_get_task_name(int8_t task_id);
```

**schedule_event**

```c
esp_err_t     schedule_event_init(void);
QueueHandle_t schedule_event_get_queue(void);   // UI 任务通过此 Queue 接收事件
```

**time_sync**

```c
esp_err_t time_sync_init(void);          // 由 wifi_manager 在获取 IP 后自动调用
bool      time_sync_is_valid(void);
bool      time_sync_is_coasting(void);   // 断网惯性走时中返回 true
esp_err_t time_sync_get_now(int *hour, int *minute, int *second);
```

## 构建与烧录

```powershell
idf.py build
idf.py flash monitor
```

NVS 结构变更后需先擦除 flash：

```powershell
idf.py erase-flash
idf.py build flash monitor
```

Wi-Fi 凭证在 `main/wifi_manager.c` 顶部修改：

```c
#define WIFI_SSID  "你的WiFi名称"
#define WIFI_PASS  "你的WiFi密码"
```

> 手机热点请开启 **2.4 GHz** 频段，ESP32-C6 不支持 5 GHz。

## 队友对接

**UI 读写日程：**

```c
// 读取全部日程
schedule_get_all(list, MAX_SCHEDULE_NUM, &count);

// 修改某条日程（index 0~19）
schedule_save_by_index(index, &cfg);
schedule_delete_by_index(index);
```

**UI 接收触发事件：**

```c
void ui_schedule_task(void *arg)
{
    QueueHandle_t queue = schedule_event_get_queue();
    schedule_event_t event;   // { task_id, hour, minute }
    while (1) {
        if (xQueueReceive(queue, &event, portMAX_DELAY) == pdTRUE) {
            const char *name = schedule_get_task_name(event.task_id);
            // 根据 task_id 显示提醒卡片、触发 AI/TTS
        }
    }
}
```
