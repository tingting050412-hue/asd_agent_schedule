#include "schedule_storage.h"

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "esp_err.h"
#include "nvs_flash.h"
#include "nvs.h"

#define SCHEDULE_NAMESPACE   "schedule"

#define KEY_HOUR             "hour"
#define KEY_MINUTE           "minute"
#define KEY_TASK_ID          "task_id"
#define KEY_TASK_NAME        "task_name"
#define KEY_ENABLED          "enabled"

static bool is_valid_schedule(int8_t hour, int8_t minute, int8_t task_id)
{
    return (hour >= 0 && hour <= 23 &&
            minute >= 0 && minute <= 59 &&
            task_id > 0);
}

esp_err_t schedule_storage_init(void)
{
    esp_err_t err = nvs_flash_init();

    /*
     * 如果 NVS 分区没有可用页，或 NVS 版本变化导致不兼容，
     * 则擦除 NVS 后重新初始化。
     */
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    return err;
}

static esp_err_t schedule_save_config(const schedule_config_t *cfg)
{
    if (cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!is_valid_schedule(cfg->hour, cfg->minute, cfg->task_id)) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err;

    printf("Opening NVS handle for writing... ");
    err = nvs_open(SCHEDULE_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        printf("Failed: %s\n", esp_err_to_name(err));
        return err;
    }
    printf("Done\n");

    err = nvs_set_i8(handle, KEY_HOUR, cfg->hour);
    if (err != ESP_OK) goto save_failed;

    err = nvs_set_i8(handle, KEY_MINUTE, cfg->minute);
    if (err != ESP_OK) goto save_failed;

    err = nvs_set_i8(handle, KEY_TASK_ID, cfg->task_id);
    if (err != ESP_OK) goto save_failed;

    err = nvs_set_i8(handle, KEY_ENABLED, cfg->enabled);
    if (err != ESP_OK) goto save_failed;

    err = nvs_set_str(handle, KEY_TASK_NAME, cfg->task_name);
    if (err != ESP_OK) goto save_failed;

    printf("Committing updates in NVS... ");
    err = nvs_commit(handle);
    if (err == ESP_OK) {
        printf("Done\n");
    } else {
        printf("Failed: %s\n", esp_err_to_name(err));
    }

    nvs_close(handle);
    return err;

save_failed:
    printf("Failed to save schedule: %s\n", esp_err_to_name(err));
    nvs_close(handle);
    return err;
}

static esp_err_t schedule_load_config(schedule_config_t *cfg)
{
    if (cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(cfg, 0, sizeof(schedule_config_t));

    nvs_handle_t handle;
    esp_err_t err;

    printf("Opening NVS handle for reading... ");
    err = nvs_open(SCHEDULE_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        printf("Failed: %s\n", esp_err_to_name(err));
        return err;
    }
    printf("Done\n");

    err = nvs_get_i8(handle, KEY_HOUR, &cfg->hour);
    if (err != ESP_OK) goto load_failed;

    err = nvs_get_i8(handle, KEY_MINUTE, &cfg->minute);
    if (err != ESP_OK) goto load_failed;

    err = nvs_get_i8(handle, KEY_TASK_ID, &cfg->task_id);
    if (err != ESP_OK) goto load_failed;

    err = nvs_get_i8(handle, KEY_ENABLED, &cfg->enabled);
    if (err != ESP_OK) goto load_failed;

    size_t required_size = sizeof(cfg->task_name);
    err = nvs_get_str(handle, KEY_TASK_NAME, cfg->task_name, &required_size);
    if (err != ESP_OK) goto load_failed;

    nvs_close(handle);
    return ESP_OK;

load_failed:
    nvs_close(handle);
    return err;
}

esp_err_t schedule_save_from_ui(int8_t hour,
                                int8_t minute,
                                int8_t task_id,
                                const char *task_name,
                                bool enabled)
{
    if (task_name == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    schedule_config_t cfg;
    memset(&cfg, 0, sizeof(schedule_config_t));

    cfg.hour = hour;
    cfg.minute = minute;
    cfg.task_id = task_id;
    cfg.enabled = enabled ? 1 : 0;

    strncpy(cfg.task_name, task_name, sizeof(cfg.task_name) - 1);
    cfg.task_name[sizeof(cfg.task_name) - 1] = '\0';

    return schedule_save_config(&cfg);
}

esp_err_t schedule_get_current(schedule_config_t *cfg)
{
    return schedule_load_config(cfg);
}

esp_err_t schedule_save_default(void)
{
    return schedule_save_from_ui(20, 0, TASK_BRUSH_TEETH, "睡前刷牙", true);
}
