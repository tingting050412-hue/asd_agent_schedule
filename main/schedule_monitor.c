#include "schedule_monitor.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "schedule_event.h"
#include "schedule_storage.h"
#include "time_sync.h"

#define MONITOR_TASK_NAME       "schedule_monitor"
#define MONITOR_TASK_STACK_SIZE 4096
#define MONITOR_TASK_PRIORITY   5

static int s_mock_hour = 19;
static int s_mock_minute = 59;
static int s_mock_second = 50;

static int s_last_trigger_hour[MAX_SCHEDULE_NUM];
static int s_last_trigger_minute[MAX_SCHEDULE_NUM];
static int s_last_trigger_task_id[MAX_SCHEDULE_NUM];

static TaskHandle_t s_monitor_task_handle = NULL;

static void mock_time_tick(void)
{
    s_mock_second++;

    if (s_mock_second >= 60) {
        s_mock_second = 0;
        s_mock_minute++;
    }

    if (s_mock_minute >= 60) {
        s_mock_minute = 0;
        s_mock_hour++;
    }

    if (s_mock_hour >= 24) {
        s_mock_hour = 0;
    }
}

static void trigger_state_init(void)
{
    for (int i = 0; i < MAX_SCHEDULE_NUM; i++) {
        s_last_trigger_hour[i] = -1;
        s_last_trigger_minute[i] = -1;
        s_last_trigger_task_id[i] = -1;
    }
}

static bool schedule_is_triggered(const schedule_config_t *cfg,
                                  int schedule_index,
                                  int hour,
                                  int minute)
{
    if (cfg == NULL || schedule_index < 0 || schedule_index >= MAX_SCHEDULE_NUM) {
        return false;
    }

    if (cfg->enabled != 1) {
        return false;
    }

    if (hour == cfg->hour && minute == cfg->minute) {
        if (s_last_trigger_hour[schedule_index] == hour &&
            s_last_trigger_minute[schedule_index] == minute &&
            s_last_trigger_task_id[schedule_index] == cfg->task_id) {
            return false;
        }

        s_last_trigger_hour[schedule_index] = hour;
        s_last_trigger_minute[schedule_index] = minute;
        s_last_trigger_task_id[schedule_index] = cfg->task_id;

        return true;
    }

    return false;
}

static void schedule_notify_output(const schedule_config_t *cfg,
                                   int schedule_index,
                                   int hour,
                                   int minute,
                                   int second,
                                   const char *time_source)
{
    printf("\n========== Schedule Triggered ==========\n");
    printf("Source    : %s\n", time_source);
    printf("Index     : %d\n", schedule_index);
    printf("Time      : %02d:%02d:%02d\n", hour, minute, second);
    printf("Task ID   : %" PRIi8 "\n", cfg->task_id);
    printf("Task Name : %s\n", schedule_get_task_name(cfg->task_id));
    printf("========================================\n\n");
}

static void schedule_monitor_task(void *arg)
{
    (void)arg;

    while (1) {
        int current_hour = s_mock_hour;
        int current_minute = s_mock_minute;
        int current_second = s_mock_second;
        const char *time_source = "MOCK_TIME";

        if (time_sync_is_valid() &&
            time_sync_get_now(&current_hour, &current_minute, &current_second) == ESP_OK) {
            time_source = "REAL_TIME";
        }

        schedule_config_t list[MAX_SCHEDULE_NUM];
        int count = 0;
        esp_err_t err = schedule_get_all(list, MAX_SCHEDULE_NUM, &count);

        if (err == ESP_OK) {
            printf("%s: %02d:%02d:%02d | Schedule count: %d\n",
                   time_source,
                   current_hour,
                   current_minute,
                   current_second,
                   count);

            for (int i = 0; i < count; i++) {
                printf("  [%d] %02" PRIi8 ":%02" PRIi8 " enabled=%" PRIi8 " %s\n",
                       i,
                       list[i].hour,
                       list[i].minute,
                       list[i].enabled,
                       schedule_get_task_name(list[i].task_id));

                if (schedule_is_triggered(&list[i], i, current_hour, current_minute)) {
                    schedule_notify_output(&list[i],
                                           i,
                                           current_hour,
                                           current_minute,
                                           current_second,
                                           time_source);

                    err = schedule_event_send(&list[i]);
                    if (err != ESP_OK) {
                        printf("Failed to send schedule event: %s\n", esp_err_to_name(err));
                    }
                }
            }
        } else {
            printf("Failed to read schedules: %s\n", esp_err_to_name(err));
        }

        if (time_source[0] == 'M') {
            mock_time_tick();
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

esp_err_t schedule_monitor_start(void)
{
    if (s_monitor_task_handle != NULL) {
        return ESP_OK;
    }

    trigger_state_init();

    BaseType_t ret = xTaskCreate(
        schedule_monitor_task,
        MONITOR_TASK_NAME,
        MONITOR_TASK_STACK_SIZE,
        NULL,
        MONITOR_TASK_PRIORITY,
        &s_monitor_task_handle
    );

    if (ret != pdPASS) {
        s_monitor_task_handle = NULL;
        return ESP_FAIL;
    }

    return ESP_OK;
}
