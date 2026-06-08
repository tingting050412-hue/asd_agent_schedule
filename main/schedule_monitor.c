#include "schedule_monitor.h"
#include "schedule_event.h"
#include "schedule_storage.h"

#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"

#define MONITOR_TASK_NAME       "schedule_monitor"
#define MONITOR_TASK_STACK_SIZE 4096
#define MONITOR_TASK_PRIORITY   5

static int s_mock_hour = 19;
static int s_mock_minute = 59;
static int s_mock_second = 50;

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

static bool schedule_is_triggered(const schedule_config_t *cfg)
{
    static int last_trigger_hour = -1;
    static int last_trigger_minute = -1;
    static int last_trigger_task_id = -1;

    if (cfg == NULL) {
        return false;
    }

    if (cfg->enabled != 1) {
        return false;
    }

    if (s_mock_hour == cfg->hour && s_mock_minute == cfg->minute) {
        if (last_trigger_hour == s_mock_hour &&
            last_trigger_minute == s_mock_minute &&
            last_trigger_task_id == cfg->task_id) {
            return false;
        }

        last_trigger_hour = s_mock_hour;
        last_trigger_minute = s_mock_minute;
        last_trigger_task_id = cfg->task_id;

        return true;
    }

    return false;
}

static void schedule_notify_output(const schedule_config_t *cfg)
{
    printf("\n========== Schedule Triggered ==========\n");
    printf("Time      : %02d:%02d:%02d\n", s_mock_hour, s_mock_minute, s_mock_second);
    printf("Task ID   : %" PRIi8 "\n", cfg->task_id);
    printf("Task Name : %s\n", cfg->task_name);
    printf("========================================\n\n");
}

static void schedule_monitor_task(void *arg)
{
    (void)arg;

    while (1) {
        schedule_config_t cfg;
        esp_err_t err = schedule_get_current(&cfg);

        if (err == ESP_OK) {
            printf("Mock time: %02d:%02d:%02d | Schedule: %02" PRIi8 ":%02" PRIi8 " %s\n",
                   s_mock_hour,
                   s_mock_minute,
                   s_mock_second,
                   cfg.hour,
                   cfg.minute,
                   cfg.task_name);

            if (schedule_is_triggered(&cfg)) {
                schedule_notify_output(&cfg);

                err = schedule_event_send(&cfg);
                if (err != ESP_OK) {
                    printf("Failed to send schedule event: %s\n", esp_err_to_name(err));
                }
            }
        } else {
            printf("Failed to read schedule: %s\n", esp_err_to_name(err));
        }

        mock_time_tick();

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

esp_err_t schedule_monitor_start(void)
{
    if (s_monitor_task_handle != NULL) {
        return ESP_OK;
    }

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
