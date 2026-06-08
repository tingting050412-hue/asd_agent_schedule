#ifndef SCHEDULE_STORAGE_H
#define SCHEDULE_STORAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TASK_WAKE_UP          1
#define TASK_BREAKFAST        2
#define TASK_SOCIAL_TRAIN     3
#define TASK_BRUSH_TEETH      4
#define TASK_READING          5
#define TASK_SLEEP            6

#define MAX_SCHEDULE_NUM      20

typedef struct {
    int8_t hour;
    int8_t minute;
    int8_t task_id;
    int8_t enabled;
} schedule_config_t;

esp_err_t schedule_storage_init(void);

esp_err_t schedule_save_from_ui(int8_t hour,
                                int8_t minute,
                                int8_t task_id,
                                bool enabled);

esp_err_t schedule_save_by_index(int index, const schedule_config_t *cfg);

esp_err_t schedule_get_by_index(int index, schedule_config_t *cfg);

esp_err_t schedule_get_all(schedule_config_t *list, int max_num, int *out_count);

esp_err_t schedule_delete_by_index(int index);

esp_err_t schedule_get_count(int *count);

esp_err_t schedule_get_current(schedule_config_t *cfg);

esp_err_t schedule_save_default(void);

const char *schedule_get_task_name(int8_t task_id);

#ifdef __cplusplus
}
#endif

#endif
