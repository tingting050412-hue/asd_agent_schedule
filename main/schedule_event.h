#ifndef SCHEDULE_EVENT_H
#define SCHEDULE_EVENT_H

#include <stdint.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "schedule_storage.h"
#include "time_sync.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 日程触发事件：hour / minute 即触发时的当前时间（触发条件为 current == scheduled）。
 *
 * UI 实时时钟显示：在 UI 任务中直接调用 time_sync_get_now(&h, &m, &s)
 * 即可获取当前时间，无需额外接口。
 */
typedef struct {
    int8_t task_id;
    int8_t hour;
    int8_t minute;
} schedule_event_t;

esp_err_t schedule_event_init(void);

esp_err_t schedule_event_send(const schedule_config_t *cfg);

QueueHandle_t schedule_event_get_queue(void);

#ifdef __cplusplus
}
#endif

#endif
