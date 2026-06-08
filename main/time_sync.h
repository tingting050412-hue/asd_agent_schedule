#ifndef TIME_SYNC_H
#define TIME_SYNC_H

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t time_sync_init(void);

bool time_sync_is_valid(void);

esp_err_t time_sync_get_now(int *hour, int *minute, int *second);

#ifdef __cplusplus
}
#endif

#endif
