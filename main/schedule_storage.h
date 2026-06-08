#ifndef SCHEDULE_STORAGE_H
#define SCHEDULE_STORAGE_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 日程任务 ID 建议：
 * 后续 UI、时间监控、AI/TTS 模块都应统一使用这些 ID。
 */
#define TASK_BRUSH_TEETH      1
#define TASK_SLEEP            2
#define TASK_BREAKFAST        3
#define TASK_SOCIAL_TRAIN     4
#define TASK_READING          5
#define TASK_WAKE_UP          6

typedef struct {
    int8_t hour;             // 0~23
    int8_t minute;           // 0~59
    int8_t task_id;          // 任务 ID，例如 TASK_BRUSH_TEETH
    char task_name[32];      // 任务名称，例如“睡前刷牙”
    int8_t enabled;          // 1=启用，0=关闭
} schedule_config_t;

/*
 * 初始化 NVS。
 * app_main() 中应先调用该函数。
 */
esp_err_t schedule_storage_init(void);

/*
 * UI 保存接口。
 * LVGL 的“保存”按钮回调中调用该函数。
 */
esp_err_t schedule_save_from_ui(int8_t hour,
                                int8_t minute,
                                int8_t task_id,
                                const char *task_name,
                                bool enabled);

/*
 * 读取当前保存的日程。
 * 时间监控模块、UI 初始化界面可调用该函数。
 */
esp_err_t schedule_get_current(schedule_config_t *cfg);

/*
 * 写入默认日程。
 * 当前默认值：20:00 睡前刷牙。
 */
esp_err_t schedule_save_default(void);

#ifdef __cplusplus
}
#endif

#endif // SCHEDULE_STORAGE_H
