#include "logger_config_private.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "logger_config.h"
#include "config_events.h"

#if defined(CONFIG_GPS_LOG_USE_CJSON)
#include "cJSON.h"
#else
#include "json.h"
#endif
#include "strbf.h"
#include "common_cfg.h"

#ifdef CONFIG_LOGGER_VFS_ENABLED
#include "vfs.h"
#endif
#if defined(CONFIG_GPS_LOG_ENABLED)
#include "gps_user_cfg.h"
#endif

static const char *TAG = "config";
SemaphoreHandle_t c_sem_lock = 0;
#define CFG_FILE_NAME "config.txt"
#define CFG_FILE_NAME_BACKUP "config.txt.bak"
#define CFG_FILE_NAME_DEFAULT "default.json"

static char config_file_path[PATH_MAX_CHAR_SIZE] = {0};
static char config_file_backup_path[PATH_MAX_CHAR_SIZE] = {0};
static char config_file_default_path[PATH_MAX_CHAR_SIZE] = {0};

ESP_EVENT_DEFINE_BASE(LOGGER_CONFIG_EVENT);
/// @brief List of logger config event strings
const char * const logger_config_event_strings[] = {LOGGER_CONFIG_EVENT_LIST(STRINGIFY)};

#define STAT_SCREEN_ITEM_LIST(l) l(stat_10_sec) l(stat_2_sec) l(stat_250_m) l(stat_500_m) l(stat_1852_m) l(stat_a500) l(stat_avg_10sec) l(stat_stat1) l(stat_avg_a500)
#define BOARD_LOGO_ITEM_LIST(l) l(Starboard) l(Fanatic) l(JP) l(Patrik)
#define SAIL_LOGO_ITEM_LIST(l) l(GASails) l(Duotone) l(NeilPryde) l(LoftSails) l(Gunsails) l(Point7) l(Patrik)
#define SCREEN_ROTATION_ITEM_LIST(l) l(0_deg) l(90_deg) l(180_deg) l(270_deg)
#define FW_UPDATE_CHANNEL_ITEM_LIST(l) l(stable) l(unstable)

/// @brief List of stat screen config items
static const char * const config_stat_screen_items[] = { STAT_SCREEN_ITEM_LIST(STRINGIFY) };
/// @brief Number of stat screen config items
const size_t config_stat_screen_item_count = sizeof(config_stat_screen_items) / sizeof(config_stat_screen_items[0]);
/// @brief List of speed field confio items
static const char * const config_speed_field_items[] = { SPEED_FIELD_ITEM_LIST(STRINGIFY) };
/// @brief Number of speed field config items
const size_t config_speed_field_item_count = sizeof(config_speed_field_items) / sizeof(config_speed_field_items[0]);
/// @brief List of screen config items
static const char * const config_screen_items[] = { CFG_SCREEN_ITEM_LIST(STRINGIFY) CFG_SCREEN_ITEM_LIST_A(STRINGIFY) };
/// @brief Number of screen config items
const size_t config_screen_item_count = sizeof(config_screen_items) / sizeof(config_screen_items[0]);
/// @brief List of firmwware update config items
static const char * const config_fw_update_items[] = { CFG_FW_UPDATE_ITEM_LIST(STRINGIFY) };
/// @brief Number of firmware update config items
const size_t config_fw_update_item_count = sizeof(config_fw_update_items) / sizeof(config_fw_update_items[0]);
/// @brief List of config items for the logger
const char * const config_items[] = { 
    CFG_CALIBRATION_ITEM_LIST(STRINGIFY)
    CFG_SCREEN_ITEM_LIST(STRINGIFY) 
    CFG_SCREEN_ITEM_LIST_A(STRINGIFY)
    CFG_FW_UPDATE_ITEM_LIST(STRINGIFY) 
    CFG_ITEM_LIST(STRINGIFY)
};
/// @brief Number of config items for the logger
const size_t config_item_count = sizeof(config_items) / sizeof(config_items[0]);
/// @brief Config item names as string
const char * config_item_names = ADD_QUOTE(CFG_CALIBRATION_ITEM_LIST(ADD) CFG_SCREEN_ITEM_LIST(ADD) CFG_SCREEN_ITEM_LIST_A(ADD) CFG_FW_UPDATE_ITEM_LIST(ADD) CFG_ITEM_LIST(ADD));
/// @brief List of board logo config items
static const char * const board_logos[] = {BOARD_LOGO_ITEM_LIST(STRINGIFY)};
/// @brief List of sail logo config items
static const char * const sail_logos[] = {SAIL_LOGO_ITEM_LIST(STRINGIFY)};
/// @brief List of screen rotation config items
static const char * const screen_rotations[] = {SCREEN_ROTATION_ITEM_LIST(STRINGIFY)};
/// @brief List of firmware update channel config items
static const char * const channels[] = {FW_UPDATE_CHANNEL_ITEM_LIST(STRINGIFY)};
static const char * const not_set = "not set";
const char * const seconds_list[] = {"1 sec", "2 sec", "3 sec", "4 sec", "5 sec"};

/// @brief  Get the config lock
/// @param timeout 
/// @return true if lock is taken, false otherwise
static bool cfg_lock(int timeout) {
    if (!c_sem_lock) return false;
    const TickType_t timeout_ticks = (timeout == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout);
    return  xSemaphoreTake(c_sem_lock, timeout_ticks) == pdTRUE;
}

/// @brief Release the config lock
static void cfg_unlock() {
    if (c_sem_lock) {
        xSemaphoreGive(c_sem_lock);
    }
}
/// @brief Get firmware update config item
/// @param config - config struct
/// @param num - field number
/// @param item - config item struct
/// @return - config item struct
struct m_config_item_s * get_fw_update_cfg_item(const logger_config_t *config, int num, struct m_config_item_s *item) {
    ILOG(TAG, "[%s] num:%d", __func__, num);
    assert(config);
    if(!item) return 0;
    item->name = config_fw_update_items[num];
    item->pos = num;
    if(cfg_lock(portMAX_DELAY) == pdTRUE) {
        switch(num) {
            case cfg_update_channel:
                item->value = config->fwupdate.channel;
                item->desc = channels[config->fwupdate.channel];
                break;
            case cfg_update_enabled:
                item->value = config->fwupdate.update_enabled ? 1 : 0;
                item->desc = config->fwupdate.update_enabled ? "yes" : "no";
                break;
        }
        cfg_unlock();
    }
    esp_event_post(LOGGER_CONFIG_EVENT, LOGGER_CONFIG_EVENT_CFG_GET, &num, sizeof(num), portMAX_DELAY);
    return item;
}

int set_fw_update_cfg_item(logger_config_t * config, int num) {
    ILOG(TAG, "[%s] num:%d", __func__, num);
    assert(config);
    if(num>=2) return 0;
    const char *name = config_fw_update_items[num];
    if(cfg_lock(portMAX_DELAY) == pdTRUE) {
        uint16_t val = config->fwupdate.channel;
        switch(num) {
            case cfg_update_channel:
                if(config->fwupdate.channel == 1) config->fwupdate.channel = 0;
                else config->fwupdate.channel++;
                break;
            case cfg_update_enabled:
                config->fwupdate.update_enabled = config->fwupdate.update_enabled ? 0 : 1;
                break;
        }
        cfg_unlock();
    }
    esp_event_post(LOGGER_CONFIG_EVENT, LOGGER_CONFIG_EVENT_CFG_SET, &num, sizeof(num), portMAX_DELAY);
    return 1;
}

struct m_config_item_s * get_stat_screen_cfg_item(const logger_config_t *config, int num, struct m_config_item_s *item) {
    ILOG(TAG, "[%s] num:%d", __func__, num);
    assert(config);
    if(!item) return 0;
    if(cfg_lock(portMAX_DELAY) == pdTRUE) {
        if(num>=0 && num<config_stat_screen_item_count) {
            item->name = config_stat_screen_items[num];
            item->pos = num;
            item->value = (config->screen.stat_screens & (1 << num)) ? 1 : 0;
            item->desc = item->value ? "on" : "off";
        }
        cfg_unlock();
    }
    esp_event_post(LOGGER_CONFIG_EVENT, LOGGER_CONFIG_EVENT_CFG_GET, &num, sizeof(num), portMAX_DELAY);
    return item;
}

int set_stat_screen_cfg_item(logger_config_t * config, int num) {
    ILOG(TAG, "[%s] num:%d", __func__, num);
    assert(config);
    if(num>=config_stat_screen_item_count) return 0;
    //const char *name = config_gps_items[num];
    if(cfg_lock(portMAX_DELAY) == pdTRUE) {
        uint16_t val = config->screen.stat_screens;
        if(num>=0 && num<config_stat_screen_item_count) {
            val ^= (1 << num);
        }
        if(val!=config->screen.stat_screens) {
            config->screen.stat_screens = val;
        }
        cfg_unlock();
    }
    esp_event_post(LOGGER_CONFIG_EVENT, LOGGER_CONFIG_EVENT_CFG_SET, &num, sizeof(num), portMAX_DELAY);
    return 1;
}
struct m_config_item_s * get_screen_cfg_item(const logger_config_t *config, int num, struct m_config_item_s *item) {
    ILOG(TAG, "[%s] num:%d", __func__, num);
    assert(config);
    if(!item) return 0;
    item->name = config_screen_items[num];
    item->pos = num;
    if(cfg_lock(portMAX_DELAY) == pdTRUE) {
        switch (num) {
            case cfg_speed_field: //])) {
                item->value = config->screen.speed_field;
                if(config->screen.speed_field < config_speed_field_item_count)
                    item->desc = config_speed_field_items[config->screen.speed_field];
                else
                    item->desc = not_set;
                break;
            case cfg_stat_screens_time: //])) {
                item->value = config->screen.stat_screens_time;
                item->desc = seconds_list[config->screen.stat_screens_time-1];
                break;
            case cfg_stat_screens: // ])) {
                item->value = config->screen.stat_screens;
                item->desc = "menu";
                break;
        #if defined(CONFIG_LCD_IS_EPD)
            case cfg_screen_move_offset: // ])) {
                item->value = config->screen_move_offset ? 1 : 0;
                item->desc = config->screen_move_offset ? "on" : "off";
                break;
        #else
            case cfg_screen_brightness: // ])) {
                item->value = config->screen_brightness;
                item->desc = item->value < 6 ? "5" : item->value <= 20 ? "20" : item->value <= 40 ? "40" : item->value <= 60 ? "60" : item->value == 80 ? "80" : "100" ;
                break;
        #endif
            case cfg_board_logo: // ])) {
                item->value = config->screen.board_logo;
                if(config->screen.board_logo > 0 && config->screen.board_logo <= lengthof(board_logos))
                    item->desc = board_logos[config->screen.board_logo-1];
                else
                    item->desc = not_set;
                break;
            case cfg_sail_logo: //])) {
                item->value = config->screen.sail_logo;
                if(config->screen.sail_logo > 0 && config->screen.sail_logo <= lengthof(sail_logos))
                    item->desc = sail_logos[config->screen.sail_logo-1];
                else
                    item->desc = not_set;
                break;
            case cfg_screen_rotation: // ])) {
                item->value = config->screen.screen_rotation;
                if(config->screen.screen_rotation >=0 && config->screen.screen_rotation <= lengthof(screen_rotations))
                    item->desc = screen_rotations[config->screen.screen_rotation];
                else
                    item->desc = not_set;
                break;
            default:
                item->desc = not_set;
                break;
        }
        cfg_unlock();
    }
    esp_event_post(LOGGER_CONFIG_EVENT, LOGGER_CONFIG_EVENT_CFG_GET, &num, sizeof(num), portMAX_DELAY);
    return item;
}

int set_screen_cfg_item(logger_config_t * config, int num) {
    ILOG(TAG, "[%s] num:%d", __func__, num);
    assert(config);
    if(num>=config_screen_item_count) return 0;
    const char *name = config_screen_items[num];
    int ret = 0;
    if(cfg_lock(portMAX_DELAY) == pdTRUE) {
        switch(num) {
            case cfg_speed_field:
                if(config->screen.speed_field >= config_speed_field_item_count-1) config->screen.speed_field = 0;
                else ++config->screen.speed_field;
                ret = cfg_speed_field;
                break;
            case cfg_stat_screens_time:
                if(config->screen.stat_screens_time == 1) config->screen.stat_screens_time = lengthof(seconds_list);
                else --config->screen.stat_screens_time;
                ret = cfg_stat_screens_time;
                break;
        #if defined(CONFIG_LCD_IS_EPD)
            case cfg_screen_move_offset: // ])) {
                config->screen_move_offset = config->screen_move_offset ? 0 : 1;
                ret = cfg_screen_move_offset;
                break;
        #else
            case cfg_screen_brightness: // ])) {
                if(config->screen_brightness == 100) config->screen_brightness = 80;
                else if(config->screen_brightness == 80) config->screen_brightness = 60;
                else if(config->screen_brightness == 60) config->screen_brightness = 40;
                else if(config->screen_brightness == 40) config->screen_brightness = 20;
                else if(config->screen_brightness == 20) config->screen_brightness = 5;
                else config->screen_brightness = 100;
                ret = cfg_screen_brightness;
                break;
        #endif
            case cfg_board_logo: // ])) {
                if(config->screen.board_logo >= lengthof(board_logos)) config->screen.board_logo = 1;
                else ++config->screen.board_logo;
                ret = cfg_board_logo;
                break;
            case cfg_sail_logo: // ])) {
                if(config->screen.sail_logo >= lengthof(sail_logos)) config->screen.sail_logo = 1;
                else ++config->screen.sail_logo;
                ret = cfg_sail_logo;
                break;
            case cfg_screen_rotation: // ])) {
                if(config->screen.screen_rotation >= lengthof(screen_rotations)) config->screen.screen_rotation = 0;
                else ++config->screen.screen_rotation;
                ret = cfg_screen_rotation;
                break;
            default:
                break;
        }
        cfg_unlock();
    }
    esp_event_post(LOGGER_CONFIG_EVENT, LOGGER_CONFIG_EVENT_CFG_SET, &num, sizeof(num), portMAX_DELAY);
    return ret;
}

esp_err_t config_set_screen_cb(logger_config_t * config, void(*cb)(const char *)) {
    if(!config) return ESP_ERR_INVALID_ARG;
    config->config_changed_screen_cb = cb;
    return ESP_OK;
}

logger_config_t *config_new() {
    logger_config_t * c = calloc(1, sizeof(logger_config_t));
    return config_init(c);
}

void config_delete(logger_config_t *config) {
    free(config);
}

logger_config_t *config_init(logger_config_t *config) {
    logger_config_t cf = LOGGER_CONFIG_DEFAULTS();
    memcpy(config, &cf, sizeof(logger_config_t));
    if(!c_sem_lock)
        c_sem_lock = xSemaphoreCreateRecursiveMutex();
    strbf_t buf;
    strbf_inits(&buf, config_file_path, 64);
    strbf_put_path(&buf, vfs_ctx.parts[vfs_ctx.config_part].mount_point);
    strbf_put_path(&buf, CFG_FILE_NAME);
    strbf_inits(&buf, config_file_backup_path, 64);
    strbf_put_path(&buf, vfs_ctx.parts[vfs_ctx.config_part].mount_point);
    strbf_put_path(&buf, CFG_FILE_NAME_BACKUP);
    strbf_inits(&buf, config_file_default_path, 64);
    strbf_put_path(&buf, vfs_ctx.parts[vfs_ctx.config_part].mount_point);
    strbf_put_path(&buf, CFG_FILE_NAME_DEFAULT);
    esp_event_post(LOGGER_CONFIG_EVENT, LOGGER_CONFIG_EVENT_INIT_DONE, config, sizeof(logger_config_t), portMAX_DELAY);
    return config;
}

void config_deinit(logger_config_t *config) {
    if(c_sem_lock){
        vSemaphoreDelete(c_sem_lock);
        c_sem_lock = 0;
    }
}

logger_config_t *config_defaults(logger_config_t *config) {
    ILOG(TAG,"[%s]",__func__);
    return config;
}

logger_config_t *config_clone(logger_config_t *orig, logger_config_t *config) {
    ILOG(TAG,"[%s]",__func__);
    if (!orig || !config)
        return config;
    memcpy(config, orig, sizeof(logger_config_t));
    return config;
}

uint8_t cfg_get_pos(const char *str) {
    ILOG(TAG, "[%s] str: %s", __func__, str ? str : "-");
    if (!str) {
        return 254;
    }
    for (uint8_t i = 0; i < config_item_count; i++) {
        if (!strcmp(str, config_items[i])) {
            return i;
        }
    }
    if(!strcmp(str, "Stat_screens")) {
        return cfg_stat_screens;
    }
    if(!strcmp(str, "Stat_screens_time")) {
        return cfg_stat_screens_time;
    }
    if(!strcmp(str, "GPIO12_screens")) {
        return cfg_gpio12_screens;
    }
    if(!strcmp(str, "Board_Logo") || !strcmp(str, "board_Logo")) {
        return cfg_board_logo;
    }
    if(!strcmp(str, "Sail_Logo") || !strcmp(str, "sail_Logo")) {
        return cfg_sail_logo;
    }
    if(!strcmp(str, "Screen_rotation")) {
        return cfg_screen_rotation;
    }
#if !defined(CONFIG_LCD_IS_EPD)
    if(!strcmp(str, "Screen_brightness")) {
        return cfg_screen_brightness;
    }
#endif
    if(!strcmp(str, "Sleep_info")) {
        return cfg_sleep_info;
    }
    return 255;
}

uint8_t cnf_set_item(logger_config_t *config, uint8_t pos, void * el, uint8_t force) {
    ILOG(TAG, "[%s] pos: %hhu", __func__, pos);
    if (!el) {
        return 254;
    }
    if(pos > CFG_GPS_ITEM_BASE) {
        return 255;
    }
    uint8_t changed = 255, num = 0, ret = 0;
#if defined(CONFIG_GPS_LOG_USE_CJSON)
    cJSON *value = (cJSON *)el;
#else
    JsonNode *value = (JsonNode *)el;
#endif
    if(cfg_lock(portMAX_DELAY) == pdTRUE) {
        switch(pos) {
    #ifdef USE_CUSTOM_CALIBRATION_VAL
        case cfg_cal_bat: // ])) {  // calibration for read out bat voltage
            if (value->tag != JSON_NUMBER) {
                goto err;
            }
            float val = value->data.number_;
            if (force || val != config->cal_bat) {
                config->cal_bat = value->data.number_;
                if (!str) {
                    if (m_context_rtc.RTC_calibration_bat != config->cal_bat)
                        m_context_rtc.RTC_calibration_bat = config->cal_bat;
                }
                changed = 1;
            }

            break;
    #endif
        case cfg_speed_field: // choice for first field in speed screen !!!
            ret = set_hhu(value, &config->screen.speed_field, 0);
            if(!ret) changed = cfg_speed_field;
            break;
        case cfg_speed_large_font: // fonts on the first line are bigger, actual speed font is smaller
            ret = set_hhu(value, &config->screen.speed_large_font, 0);
            if(!ret) changed = cfg_speed_large_font;
            break;
        case cfg_stat_screens: // choice for stats field when no speed, here stat_screen 1, 2 and 3 will be active
            ret = set_u(value, &config->screen.stat_screens, 0);
            if(!ret) changed = cfg_stat_screens;
            break;
        case cfg_stat_screens_time: // time between switching stat_screens
            ret = set_hhu(value, &config->screen.stat_screens_time, 0);
            if(!ret) changed = cfg_stat_screens_time;
            break;
        case cfg_gpio12_screens: // choice for stats field when gpio12 is activated (pull-up high, low = active)
            ret = set_u(value, &config->screen.gpio12_screens, 0);
            if(!ret) changed = cfg_gpio12_screens;
            break;
    #if defined(CONFIG_LCD_IS_EPD)
        case cfg_screen_move_offset:
            ret = set_hhu(value, (uint8_t*)&config->screen_move_offset, 0);
            if(!ret) changed = cfg_screen_move_offset;
            break;
    #else
        case cfg_screen_brightness: // max speed in m/s for showing Stat screens
            ret = set_hhu(value, &config->screen_brightness, 0);
            if(!ret) changed = cfg_screen_brightness;
            break;
    #endif
        case cfg_board_logo:
            ret = set_hhu(value, &config->screen.board_logo, 0);
            if(!ret) changed = cfg_board_logo;
            break;
        case cfg_sail_logo:
            ret = set_hhu(value, &config->screen.sail_logo, 0);
            if(!ret) changed = cfg_sail_logo;
            break;
        case cfg_stat_speed: // max speed in m/s for showing Stat screens
            ret = set_hhu(value, &config->screen.stat_speed, 0);
            if(!ret) changed = cfg_stat_speed;
            break;
        case cfg_bar_length: // choice for bar indicator for length of run in m (nautical mile)
            ret = set_u(value, &config->bar_length, 0);
            if(!ret) changed = cfg_bar_length;
            break;
        case cfg_archive_days: // how many days files will be moved to the "Archive" dir
            ret = set_u(value, &config->archive_days, 0);
            if(!ret) changed = cfg_archive_days;
            break;
        case cfg_update_enabled:
            ret = set_hhu(value, (uint8_t*)&config->fwupdate.update_enabled, 0);
            if(!ret) changed = cfg_update_enabled;
            break;
        case cfg_update_channel:
            ret = set_hhu(value, (uint8_t*)&config->fwupdate.channel, 0);
            if(!ret) changed = cfg_update_channel;
            break; 
        
        case cfg_screen_rotation:
            ret = set_hhu(value, (uint8_t*)&config->screen.screen_rotation, 0);
            if(!ret) changed = cfg_screen_rotation;
            break;  // type of filenaming, with MAC adress or datetime
        case cfg_sleep_info:
            ret = set_c(value, &config->sleep_info[0], 0);
            if(!ret) changed = cfg_sleep_info;
            break;
        case cfg_ssid:
            num = 0;
            goto process_sid;
        case cfg_ssid1:
            num = 1;
            goto process_sid;
        case cfg_ssid2:
            num = 2;
            goto process_sid;
        case cfg_ssid3:
            num = 3;
            process_sid:
            ret = set_c(value, &config->wifi_sta[num].ssid[0], 0);
            if(!ret) {
                if (num == 1) changed = cfg_ssid1;
                else if(num == 2) changed = cfg_ssid2;
                else if(num == 3) changed = cfg_ssid3;
                else changed = cfg_ssid;
            }
            break;
        case cfg_password:
            num = 0;
            goto process_password;
        case cfg_password1:
            num = 1;
            goto process_password;
        case cfg_password2:
            num = 2;
            goto process_password;
        case cfg_password3:
            num = 3;
            process_password:
            ret = set_c(value, &config->wifi_sta[num].password[0], 0);
            if(!ret) {
                if (num == 1) changed = cfg_password1;
                else if(num == 2) changed = cfg_password2;
                else if(num == 3) changed = cfg_password3;
                else changed = cfg_password;
            }
            break;
        case cfg_hostname:
            ret = set_c(value, &config->hostname[0], 0);
            if(!ret) changed = cfg_hostname;
            break;
        default:
            changed = 253;
            break;
        }
        cfg_unlock();
    }
    if(changed < 253)
        esp_event_post(LOGGER_CONFIG_EVENT, LOGGER_CONFIG_EVENT_CFG_CHANGED, &changed, sizeof(changed), portMAX_DELAY);
    return changed;
}

const char * config_get_var_name(const char * str, void * root) {
    if(!root) return 0;
    const char *var = 0;
    if(str) {
        var = str;
    } else {
#if defined(CONFIG_GPS_LOG_USE_CJSON)
        cJSON *name = cJSON_GetObjectItemCaseSensitive(root, "name");
        var = name->valuestring;
#else
        JsonNode *name = json_find_member((JsonNode*)root, "name");
        var = name->data.string_;
#endif
    }
    return var;
}

int config_set(logger_config_t *config, const char *str, void *root, uint8_t force) {
    ILOG(TAG,"[%s] name: %s",__func__, str ? str : "-");
    if (!root) return 254;
    uint8_t changed = 255;
#if defined(CONFIG_GPS_LOG_USE_CJSON)
    cJSON *name = 0, *value = 0;
#else
    JsonNode *name = 0, *value = 0;
#endif
    const char *var = 0;
    if(str) {
        var = str;
#if defined(CONFIG_GPS_LOG_USE_CJSON)
        value = cJSON_GetObjectItemCaseSensitive(root, str);
#else
        value = json_find_member((JsonNode*)root, str);
#endif
    }
    else {
#if defined(CONFIG_GPS_LOG_USE_CJSON)
        value = cJSON_GetObjectItemCaseSensitive(root, "value");
        name = cJSON_GetObjectItemCaseSensitive(root, "name");
        var = name->valuestring;
#else
        value = json_find_member((JsonNode*)root, "value");
        name = json_find_member((JsonNode*)root, "name");
        var = name->data.string_;
#endif
    }
    uint8_t pos = cfg_get_pos(var);
    if (pos >= 254) {
        DLOG(TAG, "[%s] ! var\n", __func__);
        changed = 255;
        goto err;
    }
    changed = cnf_set_item(config, pos, value, force);

    if (config->config_changed_screen_cb && changed < config_item_count)
        config->config_changed_screen_cb(var);
err:
    return changed;
}

int config_set_var(logger_config_t *config, const char *json, const char *var) {
    ILOG(TAG, "[%s] '%s'", __func__, json ? json : var ? var : "-");
#if defined(CONFIG_GPS_LOG_USE_CJSON)
    cJSON *root = cJSON_Parse(json);
#else
    JsonNode *root = json_decode(json);
#endif
    if (!root) {
        return -1;
    }
    int ret = -1;
    ret = gps_config_set(var, root, 0);
    if(ret == 255)
        ret = config_set(config, var, root, 0);
#if defined(CONFIG_GPS_LOG_USE_CJSON)
    cJSON_Delete(root);
#else
    json_delete(root);
#endif
    return ret;
}

int config_save_var(struct logger_config_s *config, const char *json, const char *var) {
    ILOG(TAG,"[%s] name: %s",__func__, var ? var : "-");
    IMEAS_START();
    int ret = -1;
    ret = config_set_var(config, json, var);
    if (ret >= 0) {
        ret = config_save_json(config);
    }
    IMEAS_END(TAG, "[%s] took %llu us", __func__);
    return ret;
}

int config_save_var_b(logger_config_t *config, const char *json) {
    ILOG(TAG,"[%s]",__func__);
    return config_save_var(config, json, 0);
}

esp_err_t config_decode(logger_config_t *config, const char *json) {
    ILOG(TAG,"[%s]",__func__);
    int ret = ESP_OK, changed;
    gps_config_decode(json);
#if defined(CONFIG_GPS_LOG_USE_CJSON)
    cJSON *root = cJSON_Parse(json), *item = 0;
#else
    JsonNode *root = json_decode(json), *item = 0;
#endif    
    if (!root) {
        return ESP_FAIL;
    }
    for (int i = 0; i < config_item_count; ++i) {
#if defined(CONFIG_GPS_LOG_USE_CJSON)
        item = cJSON_GetObjectItemCaseSensitive(root, config_items[i]);
#else
        item = json_find_member(root, config_items[i]);
#endif
        if(!item) {
#if defined(CONFIG_GPS_LOG_USE_CJSON)
            if(i==cfg_stat_screens) {
                item = cJSON_GetObjectItemCaseSensitive(root, "Stat_screens");
            } else if(i==cfg_stat_screens_time) {
                item = cJSON_GetObjectItemCaseSensitive(root, "Stat_screens_time");
            } else if(i==cfg_gpio12_screens) {
                item = cJSON_GetObjectItemCaseSensitive(root, "GPIO12_screens");
            } else if(i==cfg_board_logo) {
                item = cJSON_GetObjectItemCaseSensitive(root, "board_Logo");
            } else if(i==cfg_sail_logo) {
                item = cJSON_GetObjectItemCaseSensitive(root, "sail_Logo");
            } else if(i==cfg_sleep_info) {
                item = cJSON_GetObjectItemCaseSensitive(root, "Sleep_info");
            }
#else
            if(i==cfg_stat_screens) {
                item = json_find_member(root, "Stat_screens");
            } else if(i==cfg_stat_screens_time) {
                item = json_find_member(root, "Stat_screens_time");
            } else if(i==cfg_gpio12_screens) {
                item = json_find_member(root, "GPIO12_screens");
            } else if(i==cfg_board_logo) {
                item = json_find_member(root, "board_Logo");
            } else if(i==cfg_sail_logo) {
                item = json_find_member(root, "sail_Logo");
            } else if(i==cfg_sleep_info) {
                item = json_find_member(root, "Sleep_info");
            }
#endif
        }
        if(item) {
            cnf_set_item(config, i, item, 0);
        }
    }
#if defined(CONFIG_GPS_LOG_USE_CJSON)
    cJSON_Delete(root);
#else
    json_delete(root);
#endif
    return ret;
#undef SET_CONF
}

esp_err_t config_load_json(logger_config_t *config) {
    ILOG(TAG,"[%s]",__func__);
    IMEAS_START();
    int ret = ESP_OK;
    char *json = 0;
    // if(cfg_lock(portMAX_DELAY) == pdTRUE) {
#ifdef CONFIG_LOGGER_VFS_ENABLED
    if ((json = s_read_from_file(config_file_path, 0))) {
        ILOG(TAG,"[%s] from %s done",__func__, config_file_path);
    } else if ((json = s_read_from_file(config_file_backup_path, 0))) {
        ILOG(TAG,"[%s] from %s done",__func__, config_file_backup_path);
    } else 
#endif
    {
        ESP_LOGE(TAG, "configuration not found...");
        goto done;
    }
    DLOG(TAG, "[%s] json for load: %s\n", __func__ , json);
    ret = config_decode(config, json);
done:
    // cfg_unlock();
    // }
    if (json)
        free(json);
    esp_event_post(LOGGER_CONFIG_EVENT, LOGGER_CONFIG_EVENT_LOAD_DONE, config, sizeof(logger_config_t), portMAX_DELAY);
    IMEAS_END(TAG, "[%s] took %llu us", __func__);
    return ret;
}

esp_err_t config_save_json(logger_config_t *config) {
    ILOG(TAG,"[%s]",__func__);
    int ret = ESP_OK;
    strbf_t sb;
    strbf_init(&sb);
    char *json = config_encode_json(config, &sb);
#if defined(CONFIG_GPS_LOG_USE_CJSON)
    cJSON *root = cJSON_Parse(json);
#else
    JsonNode *root = json_decode(json);
#endif
    if (!root) {
        ESP_LOGE(TAG, "[%s] bad json: %s", __func__ , json);
        goto done;
    } else {
#ifdef CONFIG_GPS_LOG_USE_CJSON
        cJSON_Delete(root);
#else
        json_delete(root);
#endif
    }
    DLOG(TAG, "[%s] save json: %s\n", __func__, json);
#ifdef CONFIG_LOGGER_VFS_ENABLED
    s_rename_file_n(config_file_path, config_file_backup_path, 1);
    ret = s_write(config_file_path, 0, sb.start, sb.cur - sb.start);
#endif
done:
    strbf_free(&sb);
    esp_event_post(LOGGER_CONFIG_EVENT, ret>=0 ? LOGGER_CONFIG_EVENT_SAVE_DONE : LOGGER_CONFIG_EVENT_SAVE_FAIL, config, sizeof(logger_config_t), portMAX_DELAY);
    return ret;
}

logger_config_t *config_fix_values(logger_config_t *config) {
    ILOG(TAG,"[%s]",__func__);
    if (config->screen.stat_screens_time < 1)
        config->screen.stat_screens_time = 1;
    return config;
}

int config_compare(logger_config_t *orig, logger_config_t *config) {
    ILOG(TAG,"[%s]",__func__);
    if (!orig || !config)
        return -1;
    if (orig && !config)
        return -2;
    if (!orig && config)
        return -3;
    if(orig && config) {
        if (orig->screen.speed_field != config->screen.speed_field)
            return cfg_speed_field;
        if (orig->screen.speed_large_font != config->screen.speed_large_font)
            return cfg_speed_large_font;
        if (orig->bar_length != config->bar_length)
            return cfg_bar_length;
        if (orig->screen.stat_speed != config->screen.stat_speed)
            return cfg_stat_speed;
        if (orig->archive_days != config->archive_days)
            return cfg_archive_days;
        if (orig->screen.stat_screens_time != config->screen.stat_screens_time)
            return cfg_stat_screens_time;
        if (orig->screen.stat_screens != config->screen.stat_screens)
            return cfg_stat_screens;
        if (orig->screen.gpio12_screens != config->screen.gpio12_screens)
            return cfg_gpio12_screens;
#if defined(CONFIG_LCD_IS_EPD)
        if (orig->screen_move_offset != config->screen_move_offset)
            return cfg_screen_move_offset;
#else
        if (orig->screen_brightness != config->screen_brightness)
            return cfg_screen_brightness;
#endif
        if (orig->screen.board_logo != config->screen.board_logo)
            return cfg_board_logo;
        if (orig->screen.sail_logo != config->screen.sail_logo)
            return cfg_sail_logo;
        if (strcmp(config->sleep_info, orig->sleep_info))
            return cfg_sleep_info;
        if (strcmp(config->hostname, orig->hostname))
            return cfg_hostname;
        if (orig->speed_field_count != config->speed_field_count)
            return cfg_hostname+1;
        if (config->screen.screen_rotation != orig->screen.screen_rotation)
            return cfg_screen_rotation;
        if (config->fwupdate.update_enabled != orig->fwupdate.update_enabled)
            return cfg_update_enabled;
        if (config->fwupdate.channel != orig->fwupdate.channel)
            return cfg_update_channel;
        for(uint8_t i=0,j=L_CONFIG_SSID_MAX,k=cfg_ssid; i<j; i++,k+=2) {
            if (strcmp(config->wifi_sta[i].ssid, orig->wifi_sta[i].ssid))
                return k;
            if (strcmp(config->wifi_sta[i].password, orig->wifi_sta[i].password))
                return k+1;
        }
    }
    return 0;
}
static const char * const cfg_values[] = {
    ",\"values\":[",
    "{\"value\":",
    ",\"title\":\"",
};

static uint8_t add_from_list(strbf_t *lsb, const char * const *list, size_t len, uint8_t initvalue) {
    strbf_puts(lsb, cfg_values[0]);
    for(uint8_t i = 0, j = len; i < j; i++) {
        strbf_puts(lsb, cfg_values[1]);
        strbf_putn(lsb, i + initvalue);
        strbf_puts(lsb, cfg_values[2]);
        strbf_puts(lsb, list[i]);
        strbf_puts(lsb, "\"}");
        if(i < j-1) strbf_putc(lsb, ',');
    }
    strbf_puts(lsb, "]");
    return 0;
}

uint8_t cnf_get_item(const logger_config_t *config, uint8_t pos, strbf_t * lsb, uint8_t mode) {
    ILOG(TAG,"[%s] pos: %hhu",__func__, pos);
    if (!lsb) return 254;
    if(pos >= config_item_count) {
        return 255;
    }
    const char * start = lsb->cur;
    uint8_t num = 0;
    if (mode)
        strbf_puts(lsb, "{\"name\":");
    strbf_puts(lsb, "\"");
    strbf_puts(lsb, config_items[pos]);
    strbf_puts(lsb, "\"");
    if (mode)
        strbf_puts(lsb, ",\"value\"");
    strbf_putc(lsb, ':');
    if(cfg_lock(portMAX_DELAY) == pdTRUE) {
        switch (pos) {
        #ifdef USE_CUSTOM_CALIBRATION_VAL
            case cfg_cal_bat: // calibration for read out bat voltage
                strbf_putd(lsb, config->cal_bat, 1, 4);
                if (mode) {
                    strbf_puts(lsb, ",\"info\":\"calibration for read out bat voltage\",\"type\":\"float\",\"ext\":\"V\"");
                }
            break;
        #endif
            case cfg_speed_field: // choice for first field in speed screen !!!
                strbf_putn(lsb, config->screen.speed_field);
                if (mode) {
                    strbf_puts(lsb, ",\"info\":\"choice for first field in speed screen\",\"type\":\"int\"");
                    add_from_list(lsb, config_speed_field_items, lengthof(config_speed_field_items), 0);
                }
                break;
            case cfg_speed_large_font: // fonts on the first line are bigger, actual speed font is smaller
                strbf_putn(lsb, config->screen.speed_large_font);
                if (mode) {
                    strbf_puts(lsb, ",\"info\":\"fonts on the first line are bigger, actual speed font is smaller\",\"type\":\"bool\"");
                }
                break;
            case cfg_stat_screens: // choice for stats field when no speed, here stat_screen 1, 2 and 3 will be active
                strbf_putn(lsb, config->screen.stat_screens);
                if (mode) {
                    strbf_puts(lsb, ",\"info\":\"Stat_screens choice : activate / deactivate screens to show.\",\"type\":\"int\"");
                    strbf_puts(lsb, ",\"toggles\":[");
                    uint16_t j = 1;
                    for(uint8_t i= 0, k = config_stat_screen_item_count; i < k; i++, j <<= 1) {
                        strbf_puts(lsb, "{\"pos\":");
                        strbf_putn(lsb, i);
                        strbf_puts(lsb, cfg_values[2]);
                        strbf_puts(lsb, config_stat_screen_items[i]);
                        strbf_puts(lsb, "\",\"value\":");
                        strbf_putn(lsb, j);
                        strbf_puts(lsb, "}");
                        if(i < k-1) strbf_putc(lsb, ',');
                    }
                    strbf_puts(lsb, "]");
                }
                break;
            case cfg_stat_screens_time: // time between switching stat_screens
                strbf_putn(lsb, config->screen.stat_screens_time);
                if (mode) {
                    strbf_puts(lsb, ",\"info\":\"The time between toggle the different stat screens\",\"type\":\"int\"");
                    add_from_list(lsb, seconds_list, lengthof(seconds_list), 1);
                }
                break;
            case cfg_gpio12_screens: // choice for stats field when gpio12 is activated (pull-up high, low = active)
                strbf_putn(lsb, config->screen.gpio12_screens);
                if (mode) {
                    strbf_puts(lsb, ",\"info\":\"GPIO12_screens choice : Every digit shows the according GPIO_screen after each push. Screen 4 = s10 runs, screen 5 = alfa's.\",\"type\":\"int\"");
                }
                break;
        #if defined(CONFIG_LCD_IS_EPD)
            case cfg_screen_move_offset:
                strbf_putn(lsb, config->screen_move_offset);
                if (mode) {
                    strbf_puts(lsb, ",\"info\":\"move epd sceen content to pervent panel burn\",\"type\":\"bool\"");
                }
                break;
        #else
            case cfg_screen_brightness:
                strbf_putn(lsb, config->screen_brightness);
                if (mode) {
                    strbf_puts(lsb, ",\"info\":\"Display brightness\",\"type\":\"int\"");
                    strbf_puts(lsb, cfg_values[0]);
                    for(uint8_t i = 0, j = 105, step = 5; i < j; i+=step) {
                        if(i == 5 || i % 20 == 0) {
                            strbf_puts(lsb, cfg_values[1]);
                            strbf_putn(lsb, i);
                            strbf_puts(lsb, cfg_values[2]);
                            strbf_puts(lsb, i==5 ? "0" : i==20 ? "20" : i==40 ? "40" : i==60 ? "60" : i==80 ? "80" : "100");
                            strbf_puts(lsb, "\"}");
                            if(i < j-step) strbf_putc(lsb, ',');
                        }
                    }
                    strbf_puts(lsb, "]");
                }
            break;
        #endif
            case cfg_board_logo:
                strbf_putn(lsb, config->screen.board_logo);
                if (mode) {
                    strbf_puts(lsb, ",\"info\":\"Board_Logo\",\"type\":\"int\"");
                    add_from_list(lsb, board_logos, lengthof(board_logos), 1);
                }
                break;
            case cfg_sail_logo:
                strbf_putn(lsb, config->screen.sail_logo);
                if (mode) {
                    strbf_puts(lsb, ",\"info\":\"Sail Logo\",\"type\":\"int\"");
                    add_from_list(lsb, sail_logos, lengthof(sail_logos), 1);
                }
                break;
            case cfg_stat_speed: // max speed in m/s for showing Stat screens
                strbf_putn(lsb, config->screen.stat_speed);
                if (mode) {
                    strbf_puts(lsb, ",\"info\":\"max speed in m/s for showing Stat screens\",\"type\":\"int\",\"ext\":\"m/s\"");
                }
                break;
            case cfg_bar_length: // choice for bar indicator for length of run in m
                // (nautical mile)
                strbf_putn(lsb, config->bar_length);
                if (mode) {
                    strbf_puts(lsb, ",\"info\":\"bar_length: Default length = 1852 m for 100% bar (=Nautical mile)\",\"type\":\"int\",\"ext\":\"m\"");
                }
                break;
            case cfg_archive_days: // how many days files will be moved to the "Archive" dir
                strbf_putn(lsb, config->archive_days);
                if (mode) {
                    strbf_puts(lsb, ",\"info\":\"how many days files will be moved to the 'Archive' dir\",\"type\":\"int\",\"ext\":\"d\"");
                }
                break;
            case cfg_update_enabled:
                strbf_putn(lsb, config->fwupdate.update_enabled);
                if (mode) {
                    strbf_puts(lsb, ",\"info\":\"wether to allow automatic firmware updates or not\",\"type\":\"bool\"");
                }
                break;
            case cfg_update_channel:
                strbf_putn(lsb, config->fwupdate.channel);
                if (mode) {
                    strbf_puts(lsb, ",\"info\":\"automatic firmware update channel\",\"type\":\"int\"");
                    strbf_puts(lsb, ",\"depends\":\"");
                    strbf_puts(lsb, config_items[cfg_update_enabled]);
                    strbf_puts(lsb, "\"");
                    add_from_list(lsb, channels, lengthof(channels), 0);
                }
                break;
            case cfg_screen_rotation:
                strbf_putn(lsb, config->screen.screen_rotation);
                if (mode) {
                    strbf_puts(lsb, ",\"info\":\"screen rotation degrees\",\"type\":\"int\"");
                    add_from_list(lsb, screen_rotations, lengthof(screen_rotations), 0);
                }
                break;
            case cfg_sleep_info:
                strbf_puts(lsb, "\"");
                strbf_puts(lsb, config->sleep_info);
                strbf_puts(lsb, "\"");
                if (mode) {
                    strbf_puts(lsb, ",\"info\":\"your preferred sleep text\",\"type\":\"str\"");
                }
                break;
            case cfg_ssid:
                num = 0;
                goto process_sid;
            case cfg_ssid1:
                num = 1;
                goto process_sid;
            case cfg_ssid2:
                num = 2;
                goto process_sid;
            case cfg_ssid3:
                num = 3;
                process_sid:
                strbf_puts(lsb, "\"");
                strbf_puts(lsb, config->wifi_sta[num].ssid);
                strbf_puts(lsb, "\"");
                if (mode) {
                    strbf_puts(lsb, ",\"info\":\"wifi ssid\",\"type\":\"str\"");
                }
                break;
            case cfg_password:
                num = 0;
                goto process_password;
            case cfg_password1:
                num = 1;
                goto process_password;
            case cfg_password2:
                num = 2;
                goto process_password;
            case cfg_password3:
                num = 3;
                process_password:
                strbf_puts(lsb, "\"");
                strbf_puts(lsb, config->wifi_sta[num].password);
                strbf_puts(lsb, "\"");
                if (mode) {
                    strbf_puts(lsb, ",\"info\":\"wifi network password\",\"type\":\"str\"");
                }
                break;
            case cfg_hostname:
                strbf_puts(lsb, "\"");
                strbf_puts(lsb, config->hostname);
                strbf_puts(lsb, "\"");
                if (mode) {
                    strbf_puts(lsb, ",\"info\":\"hostname: the hostname of the device for present itself in the network\",\"type\":\"str\"");
                }
                break;
            default:
                pos = 253;
                lsb->cur = (char*)start;
                goto err;
                break;
        }
        if (mode)
            strbf_puts(lsb, "}");
        err:
        cfg_unlock();
    }
    DLOG(TAG, "[%s] conf: %s len: %d\n", __func__, strbf_finish(lsb), lsb->cur - lsb->start);
    return pos;
}
char *config_get(const logger_config_t *config, const char *name, struct strbf_s *lsb, uint8_t mode) {
    ILOG(TAG, "[%s] %s", __func__, name ? name : "-");
    assert(lsb);
    uint8_t pos = cfg_get_pos(name);
    if(pos == 255) {
        pos = gps_cfg_get_pos(name);
    }
    if(pos >= CFG_GPS_ITEM_BASE && pos < gps_user_cfg_item_count + CFG_GPS_ITEM_BASE) {
        pos = gps_cnf_get_item(pos, lsb, mode);
    } else  if(pos < CFG_GPS_ITEM_BASE){
        pos = cnf_get_item(config, pos, lsb, mode);
    }
    return strbf_finish(lsb);
}

char *config_encode_json(logger_config_t *config, strbf_t *sb) {
    ILOG(TAG,"[%s]",__func__);
    size_t blen = BUFSIZ / 3 * 2, len = 0;
    char buf[blen], *p = 0;

    strbf_puts(sb, "{\n");
    gps_config_encode(sb, 0, 0);
    strbf_putc(sb, ',');
    for(int i = 0, j = config_item_count; i < j; i++) {
        if( (i==cfg_password && config->wifi_sta[0].password[0] == 0)
            || (i==cfg_password1 && config->wifi_sta[1].password[0] == 0)
            || (i==cfg_password2 && config->wifi_sta[2].password[0] == 0)
            || (i==cfg_password3 && config->wifi_sta[3].password[0] == 0)
            || (i==cfg_ssid && config->wifi_sta[0].ssid[0] == 0)
            || (i==cfg_ssid1 && config->wifi_sta[1].ssid[0] == 0)
            || (i==cfg_ssid2 && config->wifi_sta[2].ssid[0] == 0)
            || (i==cfg_ssid3 && config->wifi_sta[3].ssid[0] == 0)) {
            continue;
        }
        if(cnf_get_item(config, i, sb, 0) >= 253) {
            continue;
        }
        if(i < j-1) {
            strbf_putc(sb, ',');
        }
        strbf_putc(sb, '\n');
    }
    strbf_puts(sb, "}\n");
    return strbf_finish(sb);
}

