#include "schedule_storage.h"

#include <stdio.h>
#include <string.h>

#include "esp_err.h"
#include "nvs.h"
#include "nvs_flash.h"

#define SCHEDULE_NAMESPACE "schedule"
#define KEY_SCHEDULE_COUNT "schedule_count"
#define SCHEDULE_KEY_PREFIX "sch_"
#define SCHEDULE_KEY_MAX_LEN 12

static bool is_valid_index(int index)
{
    return (index >= 0 && index < MAX_SCHEDULE_NUM);
}

static bool is_valid_task_id(int8_t task_id)
{
    return (task_id >= TASK_MORNING_ROUTINE && task_id <= TASK_MED_EVENING);
}

static bool is_valid_schedule(const schedule_config_t *cfg)
{
    if (cfg == NULL) {
        return false;
    }

    return (cfg->hour >= 0 && cfg->hour <= 23 &&
            cfg->minute >= 0 && cfg->minute <= 59 &&
            is_valid_task_id(cfg->task_id) &&
            (cfg->enabled == 0 || cfg->enabled == 1));
}

static void make_schedule_key(int index, char *key, size_t key_size)
{
    snprintf(key, key_size, "%s%d", SCHEDULE_KEY_PREFIX, index);
}

static esp_err_t read_count_from_handle(nvs_handle_t handle, int *count)
{
    int32_t stored_count = 0;
    esp_err_t err;

    if (count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    err = nvs_get_i32(handle, KEY_SCHEDULE_COUNT, &stored_count);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        stored_count = 0;
        err = ESP_OK;
    }
    if (err != ESP_OK) {
        return err;
    }

    if (stored_count < 0) {
        stored_count = 0;
    } else if (stored_count > MAX_SCHEDULE_NUM) {
        stored_count = MAX_SCHEDULE_NUM;
    }

    *count = (int)stored_count;
    return ESP_OK;
}

static esp_err_t write_count_to_handle(nvs_handle_t handle, int count)
{
    if (count < 0) {
        count = 0;
    } else if (count > MAX_SCHEDULE_NUM) {
        count = MAX_SCHEDULE_NUM;
    }

    return nvs_set_i32(handle, KEY_SCHEDULE_COUNT, count);
}

static esp_err_t erase_schedule_namespace(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(SCHEDULE_NAMESPACE, NVS_READWRITE, &handle);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_erase_all(handle);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);
    return err;
}

esp_err_t schedule_storage_init(void)
{
    esp_err_t err = nvs_flash_init();

    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    if (err == ESP_OK) {
        schedule_config_t list[MAX_SCHEDULE_NUM];
        int count = 0;

        err = schedule_get_all(list, MAX_SCHEDULE_NUM, &count);
        if (err == ESP_ERR_INVALID_SIZE || err == ESP_ERR_INVALID_ARG) {
            printf("Stored schedules are incompatible. Rebuilding defaults.\n");

            err = erase_schedule_namespace();
            if (err == ESP_OK) {
                err = schedule_save_default();
            }
        }
    }

    return err;
}

esp_err_t schedule_save_by_index(int index, const schedule_config_t *cfg)
{
    if (!is_valid_index(index) || !is_valid_schedule(cfg)) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err;
    char key[SCHEDULE_KEY_MAX_LEN];
    int count = 0;

    make_schedule_key(index, key, sizeof(key));

    err = nvs_open(SCHEDULE_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = read_count_from_handle(handle, &count);
    if (err != ESP_OK) {
        nvs_close(handle);
        return err;
    }

    if (index > count) {
        nvs_close(handle);
        return ESP_ERR_INVALID_ARG;
    }

    err = nvs_set_blob(handle, key, cfg, sizeof(schedule_config_t));
    if (err != ESP_OK) {
        nvs_close(handle);
        return err;
    }

    if (index >= count) {
        err = write_count_to_handle(handle, index + 1);
        if (err != ESP_OK) {
            nvs_close(handle);
            return err;
        }
    }

    err = nvs_commit(handle);
    nvs_close(handle);

    return err;
}

esp_err_t schedule_get_by_index(int index, schedule_config_t *cfg)
{
    if (!is_valid_index(index) || cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err;
    char key[SCHEDULE_KEY_MAX_LEN];
    size_t required_size = sizeof(schedule_config_t);

    memset(cfg, 0, sizeof(schedule_config_t));
    make_schedule_key(index, key, sizeof(key));

    err = nvs_open(SCHEDULE_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_get_blob(handle, key, cfg, &required_size);
    nvs_close(handle);

    if (err != ESP_OK) {
        return err;
    }
    if (required_size != sizeof(schedule_config_t)) {
        memset(cfg, 0, sizeof(schedule_config_t));
        return ESP_ERR_INVALID_SIZE;
    }
    if (!is_valid_schedule(cfg)) {
        memset(cfg, 0, sizeof(schedule_config_t));
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

esp_err_t schedule_get_all(schedule_config_t *list, int max_num, int *out_count)
{
    if (list == NULL || out_count == NULL || max_num <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err;
    int count = 0;
    int loaded = 0;

    if (max_num > MAX_SCHEDULE_NUM) {
        max_num = MAX_SCHEDULE_NUM;
    }

    memset(list, 0, sizeof(schedule_config_t) * max_num);
    *out_count = 0;

    err = nvs_open(SCHEDULE_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            return ESP_OK;
        }
        return err;
    }

    err = read_count_from_handle(handle, &count);
    if (err != ESP_OK) {
        nvs_close(handle);
        return err;
    }

    if (count > max_num) {
        count = max_num;
    }

    for (int i = 0; i < count; i++) {
        char key[SCHEDULE_KEY_MAX_LEN];
        size_t required_size = sizeof(schedule_config_t);

        make_schedule_key(i, key, sizeof(key));
        err = nvs_get_blob(handle, key, &list[loaded], &required_size);
        if (err == ESP_OK) {
            if (required_size != sizeof(schedule_config_t) ||
                !is_valid_schedule(&list[loaded])) {
                nvs_close(handle);
                return ESP_ERR_INVALID_SIZE;
            }
            loaded++;
        } else if (err != ESP_ERR_NVS_NOT_FOUND) {
            nvs_close(handle);
            return err;
        }
    }

    nvs_close(handle);
    *out_count = loaded;

    return ESP_OK;
}

esp_err_t schedule_delete_by_index(int index)
{
    if (!is_valid_index(index)) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err;
    int count = 0;

    err = nvs_open(SCHEDULE_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = read_count_from_handle(handle, &count);
    if (err != ESP_OK) {
        nvs_close(handle);
        return err;
    }

    if (index >= count) {
        nvs_close(handle);
        return ESP_ERR_NVS_NOT_FOUND;
    }

    for (int i = index; i < count - 1; i++) {
        char from_key[SCHEDULE_KEY_MAX_LEN];
        char to_key[SCHEDULE_KEY_MAX_LEN];
        schedule_config_t next_cfg;
        size_t required_size = sizeof(next_cfg);

        make_schedule_key(i + 1, from_key, sizeof(from_key));
        make_schedule_key(i, to_key, sizeof(to_key));

        err = nvs_get_blob(handle, from_key, &next_cfg, &required_size);
        if (err != ESP_OK || required_size != sizeof(next_cfg)) {
            nvs_close(handle);
            return err == ESP_OK ? ESP_ERR_INVALID_SIZE : err;
        }

        err = nvs_set_blob(handle, to_key, &next_cfg, sizeof(next_cfg));
        if (err != ESP_OK) {
            nvs_close(handle);
            return err;
        }
    }

    char last_key[SCHEDULE_KEY_MAX_LEN];
    make_schedule_key(count - 1, last_key, sizeof(last_key));

    err = nvs_erase_key(handle, last_key);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(handle);
        return err;
    }

    err = write_count_to_handle(handle, count - 1);
    if (err != ESP_OK) {
        nvs_close(handle);
        return err;
    }

    err = nvs_commit(handle);
    nvs_close(handle);

    return err;
}

esp_err_t schedule_get_count(int *count)
{
    if (count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err;

    err = nvs_open(SCHEDULE_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            *count = 0;
            return ESP_OK;
        }
        return err;
    }

    err = read_count_from_handle(handle, count);
    nvs_close(handle);

    return err;
}

esp_err_t schedule_save_from_ui(int8_t hour,
                                int8_t minute,
                                int8_t task_id,
                                bool enabled)
{
    schedule_config_t cfg = {
        .hour = hour,
        .minute = minute,
        .task_id = task_id,
        .enabled = enabled ? 1 : 0,
    };

    return schedule_save_by_index(0, &cfg);
}

esp_err_t schedule_get_current(schedule_config_t *cfg)
{
    return schedule_get_by_index(0, cfg);
}

esp_err_t schedule_save_default(void)
{
    static const schedule_config_t defaults[] = {
        { 7,  0, TASK_MORNING_ROUTINE,  1},  /* 晨起洗漱常规         */
        { 7, 15, TASK_DRESSING,         1},  /* 穿衣动作拆解         */
        { 8,  0, TASK_MED_MORNING,      0},  /* 早晨用药（默认关闭） */
        {10,  0, TASK_TOILET_AM,        1},  /* 如厕引导（上午）     */
        {10, 30, TASK_SENSORY,          0},  /* 感觉统合（默认关闭） */
        {11, 55, TASK_LUNCH_TRANSITION, 1},  /* 午餐前5分钟过渡      */
        {12,  0, TASK_LUNCH,            1},  /* 午餐用餐常规         */
        {14,  0, TASK_AFTERNOON_HUG,    1},  /* 下午放松与拥抱       */
        {15,  0, TASK_BREATHING,        1},  /* 深呼吸时间           */
        {15, 30, TASK_TOILET_PM,        1},  /* 如厕引导（下午）     */
        {16,  0, TASK_HYDRATION,        1},  /* 补充水分和零食       */
        {16, 30, TASK_AI_CHAT,          1},  /* AI谈心               */
        {16, 55, TASK_ACTIVITY_END,     1},  /* 娱乐结束倒计时       */
        {17,  0, TASK_TOY_CLEANUP,      1},  /* 玩具回自己的家       */
        {18, 30, TASK_READING,          1},  /* 绘本时间             */
        {19,  0, TASK_SHARE_MOMENT,     1},  /* 分享小行为时间       */
        {19, 15, TASK_AI_SOCIAL,        1},  /* AI模拟社交对对碰     */
        {19, 30, TASK_MED_EVENING,      0},  /* 晚间用药（默认关闭） */
        {20,  0, TASK_BEDTIME_PREP,     1},  /* 睡前准备常规         */
        {21, 12, TASK_SLEEP_RITUAL,     1},  /* 关灯睡觉仪式         */
    };

    for (int i = 0; i < (int)(sizeof(defaults) / sizeof(defaults[0])); i++) {
        esp_err_t err = schedule_save_by_index(i, &defaults[i]);
        if (err != ESP_OK) {
            return err;
        }
    }

    return ESP_OK;
}

const char *schedule_get_task_name(int8_t task_id)
{
    switch (task_id) {
    case TASK_MORNING_ROUTINE:  return "晨起洗漱常规";
    case TASK_DRESSING:         return "穿衣动作拆解";
    case TASK_TOILET_AM:        return "如厕引导（上午）";
    case TASK_LUNCH:            return "午餐用餐常规";
    case TASK_TOILET_PM:        return "如厕引导（下午）";
    case TASK_HYDRATION:        return "补充水分和零食";
    case TASK_BEDTIME_PREP:     return "睡前准备常规";
    case TASK_SLEEP_RITUAL:     return "关灯睡觉仪式";
    case TASK_LUNCH_TRANSITION: return "午餐前5分钟过渡";
    case TASK_AFTERNOON_HUG:    return "下午放松与拥抱";
    case TASK_ACTIVITY_END:     return "娱乐结束倒计时";
    case TASK_AI_CHAT:          return "AI谈心";
    case TASK_SHARE_MOMENT:     return "分享小行为时间";
    case TASK_AI_SOCIAL:        return "AI模拟社交对对碰";
    case TASK_TOY_CLEANUP:      return "玩具回自己的家";
    case TASK_READING:          return "绘本时间";
    case TASK_BREATHING:        return "深呼吸时间";
    case TASK_SENSORY:          return "感觉统合转换练习";
    case TASK_MED_MORNING:      return "早晨用药";
    case TASK_MED_EVENING:      return "晚间用药";
    default:                    return "未知日程";
    }
}
