#include <inttypes.h>
#include <stdio.h>

#include "esp_err.h"
#include "schedule_event.h"
#include "schedule_monitor.h"
#include "schedule_storage.h"
#include "time_sync.h"

static void print_schedule_list(void)
{
    schedule_config_t list[MAX_SCHEDULE_NUM];
    int count = 0;
    esp_err_t err = schedule_get_all(list, MAX_SCHEDULE_NUM, &count);

    if (err != ESP_OK) {
        printf("Failed to load schedule list: %s\n", esp_err_to_name(err));
        return;
    }

    printf("Saved schedules: %d\n", count);
    for (int i = 0; i < count; i++) {
        printf("  [%d] %02" PRIi8 ":%02" PRIi8 " %s enabled=%" PRIi8 "\n",
               i,
               list[i].hour,
               list[i].minute,
               schedule_get_task_name(list[i].task_id),
               list[i].enabled);
    }
}

void app_main(void)
{
    esp_err_t err;
    int count = 0;

    printf("\nASD Schedule Storage + Monitor Demo Start\n");

    err = schedule_storage_init();
    if (err != ESP_OK) {
        printf("Failed to initialize schedule storage: %s\n", esp_err_to_name(err));
        return;
    }

    err = schedule_get_count(&count);
    if (err != ESP_OK) {
        printf("Failed to get schedule count: %s\n", esp_err_to_name(err));
        return;
    }

    if (count == 0) {
        printf("No schedule found. Save default ASD schedules.\n");

        err = schedule_save_default();
        if (err != ESP_OK) {
            printf("Failed to save default schedules: %s\n", esp_err_to_name(err));
            return;
        }

        printf("Default schedules saved successfully.\n");
    } else {
        printf("Schedules loaded from NVS successfully.\n");
    }

    print_schedule_list();

    err = schedule_event_init();
    if (err != ESP_OK) {
        printf("Failed to initialize schedule event: %s\n", esp_err_to_name(err));
        return;
    }

    /*
     * Wi-Fi will be integrated by another module later.
     * Keep this call here for now so SNTP can start when networking is ready.
     * After Wi-Fi integration, move time_sync_init() to the Wi-Fi connected callback.
     */
    err = time_sync_init();
    if (err != ESP_OK) {
        printf("SNTP time sync is not ready: %s\n", esp_err_to_name(err));
        printf("Schedule monitor will use mock time fallback.\n");
    }

    err = schedule_monitor_start();
    if (err != ESP_OK) {
        printf("Failed to start schedule monitor: %s\n", esp_err_to_name(err));
        return;
    }

    printf("Schedule monitor started.\n");
}
