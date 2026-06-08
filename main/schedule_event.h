#ifndef SCHEDULE_EVENT_H
#define SCHEDULE_EVENT_H

#include <stdint.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "schedule_storage.h"

#ifdef __cplusplus
extern "C" {
#endif

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
