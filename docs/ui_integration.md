# UI Integration

本文档面向 LVGL UI 模块，说明如何保存日程，以及如何接收日程提醒事件。

## Save Schedule From UI

UI 设置页面需要收集：

- hour: 小时，范围 `0~23`
- minute: 分钟，范围 `0~59`
- task_id: 任务 ID
- enabled: 是否启用

保存按钮回调中调用：

```c
#include "schedule_storage.h"

void ui_save_schedule_button_cb(void)
{
    int8_t hour = 20;
    int8_t minute = 0;
    int8_t task_id = TASK_BRUSH_TEETH;
    bool enabled = true;

    esp_err_t err = schedule_save_from_ui(hour,
                                          minute,
                                          task_id,
                                          enabled);
    if (err != ESP_OK) {
        /* Show save failed state in UI. */
        return;
    }

    /* Show save success state in UI. */
}
```

UI 模块不要直接操作 NVS，应统一通过 `schedule_save_from_ui()` 写入日程。家长端只能选择固定任务类型，不能输入自定义任务名称。

`schedule_save_from_ui()` 是兼容接口，默认写入 index 0。如果 UI 后续支持日程列表，建议改用 index 接口：

```c
#include "schedule_storage.h"

void ui_save_schedule_by_index(int index)
{
    schedule_config_t cfg = {
        .hour = 20,
        .minute = 30,
        .task_id = TASK_READING,
        .enabled = 1,
    };

    esp_err_t err = schedule_save_by_index(index, &cfg);
    if (err != ESP_OK) {
        /* Show save failed state in UI. */
        return;
    }

    /* Refresh schedule list in UI. */
}
```

读取全部日程：

```c
schedule_config_t list[MAX_SCHEDULE_NUM];
int count = 0;

if (schedule_get_all(list, MAX_SCHEDULE_NUM, &count) == ESP_OK) {
    for (int i = 0; i < count; i++) {
        /* Render list[i] in UI. */
    }
}
```

删除指定日程：

```c
schedule_delete_by_index(index);
```

## Receive Schedule Events

日程触发后，`schedule_monitor` 会调用 `schedule_event_send()`，事件会进入 FreeRTOS Queue。

UI 模块可以创建一个任务接收事件：

```c
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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

UI 显示函数可以由 LVGL 模块实现：

```c
void ui_show_schedule_card(int8_t task_id,
                           const char *task_name,
                           int8_t hour,
                           int8_t minute)
{
    /* Create or update LVGL schedule reminder card here. */
}
```

## Initialization Order

推荐初始化顺序：

```text
schedule_storage_init()
    ↓
schedule_event_init()
    ↓
UI task start
    ↓
schedule_monitor_start()
```

如果 UI task 启动较晚，Queue 仍可以暂存少量事件。当前 Queue 长度为 5。

## Threading Notes

- `xQueueReceive()` 可以在 UI 辅助任务中阻塞等待事件。
- 如果 LVGL 只能在指定 UI 线程操作，应在收到事件后切换到 LVGL 安全上下文再更新界面。
- 不建议在 `schedule_monitor` 任务里直接调用 LVGL API，以免增加模块耦合。
- `schedule_event_t` 只包含 `task_id`、`hour`、`minute`，UI 收到事件后通过 `schedule_get_task_name(event.task_id)` 获取名称。
