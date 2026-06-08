#include <stdio.h>
#include <inttypes.h>

#include "esp_err.h"
#include "nvs.h"
#include "schedule_event.h"
#include "schedule_storage.h"
#include "schedule_monitor.h"
#include "time_sync.h"

static void print_schedule(const schedule_config_t *cfg)
{
    printf("Current schedule:\n");
    printf("  enabled   : %" PRIi8 "\n", cfg->enabled);
    printf("  time      : %02" PRIi8 ":%02" PRIi8 "\n", cfg->hour, cfg->minute);
    printf("  task_id   : %" PRIi8 "\n", cfg->task_id);
    printf("  task_name : %s\n", cfg->task_name);
}

void app_main(void)
{
    esp_err_t err;

    printf("\nASD Schedule Storage + Monitor Demo Start\n");

    err = schedule_storage_init();
    if (err != ESP_OK) {
        printf("Failed to initialize schedule storage: %s\n", esp_err_to_name(err));
        return;
    }

    schedule_config_t cfg;
    err = schedule_get_current(&cfg);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        printf("No schedule found. Save default schedule.\n");

        err = schedule_save_default();
        if (err != ESP_OK) {
            printf("Failed to save default schedule: %s\n", esp_err_to_name(err));
            return;
        }

        err = schedule_get_current(&cfg);
        if (err != ESP_OK) {
            printf("Failed to reload schedule: %s\n", esp_err_to_name(err));
            return;
        }

        printf("Default schedule saved successfully.\n");
        print_schedule(&cfg);

    } else if (err == ESP_OK) {
        printf("Schedule loaded from NVS successfully.\n");
        print_schedule(&cfg);

    } else {
        printf("Failed to load schedule: %s\n", esp_err_to_name(err));
        return;
    }

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
