#include "time_sync.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "esp_err.h"
#include "esp_sntp.h"

#define TIME_SYNC_SERVER_1 "ntp.aliyun.com"
#define TIME_SYNC_VALID_YEAR 2024

static bool s_time_sync_started = false;

static bool time_sync_tm_is_valid(const struct tm *timeinfo)
{
    if (timeinfo == NULL) {
        return false;
    }

    return (timeinfo->tm_year + 1900) >= TIME_SYNC_VALID_YEAR;
}

esp_err_t time_sync_init(void)
{
    setenv("TZ", "CST-8", 1);
    tzset();

    if (!s_time_sync_started) {
        /*
         * Wi-Fi should be connected before calling this function.
         * This module only starts SNTP and reads system time.
         */
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, TIME_SYNC_SERVER_1);
        esp_sntp_init();

        s_time_sync_started = true;
    }

    if (!time_sync_is_valid()) {
        printf("SNTP started. Waiting for system time to become valid.\n");
    }

    return ESP_OK;
}

bool time_sync_is_valid(void)
{
    time_t now;
    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);

    return time_sync_tm_is_valid(&timeinfo);
}

esp_err_t time_sync_get_now(int *hour, int *minute, int *second)
{
    if (hour == NULL || minute == NULL || second == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    time_t now;
    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);

    if (!time_sync_tm_is_valid(&timeinfo)) {
        return ESP_FAIL;
    }

    *hour = timeinfo.tm_hour;
    *minute = timeinfo.tm_min;
    *second = timeinfo.tm_sec;

    return ESP_OK;
}
