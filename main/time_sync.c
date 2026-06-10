#include "time_sync.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "esp_err.h"
#include "esp_sntp.h"
#include "esp_timer.h"

#define TIME_SYNC_SERVER_1  "ntp.aliyun.com"
#define TIME_SYNC_VALID_YEAR 2024

static bool    s_sntp_started   = false;

/* 惯性走时参考点：最后一次 SNTP 有效时的时间和硬件计时器值 */
static bool    s_has_reference  = false;
static int     s_ref_hour       = 0;
static int     s_ref_minute     = 0;
static int     s_ref_second     = 0;
static int64_t s_ref_timer_us   = 0;   /* esp_timer_get_time() at reference */

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

    if (!s_sntp_started) {
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, TIME_SYNC_SERVER_1);
        esp_sntp_init();
        s_sntp_started = true;
    }

    printf("SNTP started. Waiting for system time to become valid.\n");

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

    if (time_sync_tm_is_valid(&timeinfo)) {
        *hour   = timeinfo.tm_hour;
        *minute = timeinfo.tm_min;
        *second = timeinfo.tm_sec;

        /* 每次 SNTP 有效时更新参考点 */
        s_ref_hour    = *hour;
        s_ref_minute  = *minute;
        s_ref_second  = *second;
        s_ref_timer_us = esp_timer_get_time();
        s_has_reference = true;

        return ESP_OK;
    }

    /* SNTP 无效：若有参考点则惯性走时 */
    if (s_has_reference) {
        int64_t elapsed_s = (esp_timer_get_time() - s_ref_timer_us) / 1000000LL;
        int total_s = s_ref_hour * 3600 + s_ref_minute * 60 + s_ref_second
                      + (int)elapsed_s;
        total_s %= 86400;

        *hour   = total_s / 3600;
        *minute = (total_s % 3600) / 60;
        *second = total_s % 60;

        return ESP_OK;
    }

    return ESP_FAIL;
}

bool time_sync_is_coasting(void)
{
    return s_has_reference && !time_sync_is_valid();
}
