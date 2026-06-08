#include "schedule_event.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define SCHEDULE_EVENT_QUEUE_LENGTH 5

static QueueHandle_t s_schedule_event_queue = NULL;

esp_err_t schedule_event_init(void)
{
    if (s_schedule_event_queue != NULL) {
        return ESP_OK;
    }

    s_schedule_event_queue = xQueueCreate(SCHEDULE_EVENT_QUEUE_LENGTH,
                                          sizeof(schedule_event_t));
    if (s_schedule_event_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t schedule_event_send(const schedule_config_t *cfg)
{
    if (cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_schedule_event_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    schedule_event_t event = {
        .task_id = cfg->task_id,
        .hour = cfg->hour,
        .minute = cfg->minute,
    };
    strncpy(event.task_name, cfg->task_name, sizeof(event.task_name) - 1);
    event.task_name[sizeof(event.task_name) - 1] = '\0';

    if (xQueueSend(s_schedule_event_queue, &event, 0) != pdTRUE) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

QueueHandle_t schedule_event_get_queue(void)
{
    return s_schedule_event_queue;
}
