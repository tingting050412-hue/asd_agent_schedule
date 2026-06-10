#ifndef SCHEDULE_STORAGE_H
#define SCHEDULE_STORAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── 生活自理 ── */
#define TASK_MORNING_ROUTINE   10   /* 晨起洗漱常规         07:00 */
#define TASK_DRESSING          11   /* 穿衣动作拆解         07:15 */
#define TASK_TOILET_AM         12   /* 如厕引导（上午）     10:00 */
#define TASK_LUNCH             13   /* 午餐用餐常规         12:00 */
#define TASK_TOILET_PM         14   /* 如厕引导（下午）     15:30 */
#define TASK_HYDRATION         15   /* 补充水分和零食       16:00 */
#define TASK_BEDTIME_PREP      16   /* 睡前准备常规         20:00 */
#define TASK_SLEEP_RITUAL      17   /* 关灯睡觉仪式         20:30 */

/* ── 过渡缓冲 ── */
#define TASK_LUNCH_TRANSITION  20   /* 午餐前5分钟过渡      11:55 */
#define TASK_AFTERNOON_HUG     21   /* 下午放松与拥抱       14:00 */
#define TASK_ACTIVITY_END      22   /* 娱乐结束倒计时       16:55 */

/* ── 社交与语言 ── */
#define TASK_AI_CHAT           30   /* AI谈心               16:30 */
#define TASK_SHARE_MOMENT      31   /* 分享小行为时间       19:00 */
#define TASK_AI_SOCIAL         32   /* AI模拟社交对对碰     19:15 */

/* ── 认知与专注 ── */
#define TASK_TOY_CLEANUP       40   /* 玩具回自己的家       17:00 */
#define TASK_READING           41   /* 绘本时间             18:30 */

/* ── 感觉统合 ── */
#define TASK_BREATHING         50   /* 深呼吸时间           15:00 */
#define TASK_SENSORY           51   /* 感觉统合转换练习     10:30 （默认关闭） */

/* ── 用药提醒 ── */
#define TASK_MED_MORNING       60   /* 早晨用药             08:00 （默认关闭） */
#define TASK_MED_EVENING       61   /* 晚间用药             19:30 （默认关闭） */

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
