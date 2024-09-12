#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_err.h"
#include "esp_log.h"

#include "logger_config.h"
#include "ubx.h"

#include "json.h"
#include "strbf.h"
#include "vfs.h"
#include "config_events.h"
#include "logger_config_private.h"
#include "logger_common.h"
#include "vfs_fat_sdspi.h"
#if defined(CONFIG_USE_FATFS)
#include "vfs_fat_spiflash.h"
#endif

static const char *TAG = "config";
SemaphoreHandle_t c_sem_lock = 0;
#define CFG_FILE_NAME "config.txt";
#define CFG_FILE_NAME_BACKUP "config.txt.bak";
#define CFG_FILE_NAME_DEFAULT "default.json";

static const char * config_file_path = 0;
static const char * config_file_backup_path = 0;
static const char * config_file_default_path = 0;

#define lengthof(x) (sizeof(x) / sizeof((x)[0]))

ESP_EVENT_DEFINE_BASE(CONFIG_EVENT);

#define STRINGIFY_(x) #x
#define STRINGIFY(x) STRINGIFY_(x),
#define A_
#define ADD(x) x|A_
#define ADD_QUOTE(x) STRINGIFY_(x)

#define SPEED_FIELD_ITEM_LIST(l) l(dynamic) l(stat_10_sec) l(stat_alpha) l(stat_1852_m) l(stat_dist_500m) l(stat_max_2s_10s) l(stat_half_hour) l(stat_1_hour) l(stat_1h_dynamic)
#define STAT_SCREEN_ITEM_LIST(l) l(stat_10_sec) l(stat_2_sec) l(stat_250_m) l(stat_500_m) l(stat_1852_m) l(stat_alfa) l(stat_avg_10sec) l(stat_stat1) l(stat_avg_a500)

#define BOARD_LOGO_ITEM_LIST(l) l(Starboard) l(Fanatic) l(JP) l(Patrik)
#define SAIL_LOGO_ITEM_LIST(l) l(GASails) l(Duotone) l(NeilPryde) l(LoftSails) l(Gunsails) l(Point7) l(Patrik)

#define SPEED_UNIT_ITEM_LIST(l) l(m/s) l(km/h) l(knots)
#define SAMPLE_RATE_ITEM_LIST(l) l(1 Hz) l(5 Hz) l(10 Hz) l(16 Hz) l(20 Hz)
#define SCREEN_ROTATION_ITEM_LIST(l) l(0_deg) l(90_deg) l(180_deg) l(270_deg)
#define FW_UPDATE_CHANNEL_ITEM_LIST(l) l(stable) l(unstable)

#define CFG_GPS_ITEM_LIST(l) l(gnss) l(sample_rate) l(timezone) l(speed_unit) l(log_txt) l(log_ubx) l(log_sbp) l(log_gpy) l(log_gpx) l(log_ubx_nav_sat) l(dynamic_model)
#define CFG_SCREEN_ITEM_LIST(l) l(speed_field) l(stat_screens_time) l(stat_screens) l(screen_move_offset) l(board_logo) l(sail_logo) l(screen_rotation)
#define CFG_FW_UPDATE_ITEM_LIST(l) l(update_enabled) l(update_channel)
#define CFG_ITEM_LIST(l) l(speed_large_font) l(bar_length) l(stat_speed) l(archive_days) l(file_date_time) l(ssid) l(password) l(ssid1) l(password1) l(ssid2) l(password2) l(ssid3) l(password3) l(gpio12_screens) l(ubx_file) l(sleep_info) l(hostname)

const char * const config_stat_screen_items[] = { STAT_SCREEN_ITEM_LIST(STRINGIFY) };
const char * const config_speed_field_items[] = { SPEED_FIELD_ITEM_LIST(STRINGIFY) };
const char * const config_screen_items[] = { CFG_SCREEN_ITEM_LIST(STRINGIFY) };
const char * const config_fw_update_items[] = { CFG_FW_UPDATE_ITEM_LIST(STRINGIFY) };
const char * const config_gps_items[] = { CFG_GPS_ITEM_LIST(STRINGIFY) };
const char * config_item_names = ADD_QUOTE(CFG_GPS_ITEM_LIST(ADD) CFG_SCREEN_ITEM_LIST(ADD) CFG_FW_UPDATE_ITEM_LIST(ADD) CFG_ITEM_LIST(ADD));
const char * config_item_names_compat = "Stat_screens|Stat_screens_time|GPIO12_screens|Board_Logo|board_Logo|sail_Logo|Sail_Logo|logTXT|logSBP|logUBX|logUBX_nav_sat|logGPY|logGPX|UBXfile|Sleep_info|";

const char * const board_logos[] = {BOARD_LOGO_ITEM_LIST(STRINGIFY)};
const char * const sail_logos[] = {SAIL_LOGO_ITEM_LIST(STRINGIFY)};
const char * const speed_units[] = {SPEED_UNIT_ITEM_LIST(STRINGIFY)};
const char * const sample_rates[] = {SAMPLE_RATE_ITEM_LIST(STRINGIFY)};
const char * const screen_rotations[] = {SCREEN_ROTATION_ITEM_LIST(STRINGIFY)};
const char * const channels[] = {FW_UPDATE_CHANNEL_ITEM_LIST(STRINGIFY)};
const char * const not_set = "not set";

logger_config_item_t * get_fw_update_cfg_item(const logger_config_t *config, int num, logger_config_item_t *item) {
    assert(config);
    if(!item) return 0;
    item->name = config_fw_update_items[num];
    item->pos = num;
    if(!strcmp(item->name, "update_channel")) {
        item->value = config->fwupdate.channel;
        item->desc = channels[config->fwupdate.channel];
    }
    else if(!strcmp(item->name, "update_enabled")) {
        item->value = config->fwupdate.update_enabled;
        item->desc = config->fwupdate.update_enabled ? "yes" : "no";
    }
    return item;
}

int set_fw_update_cfg_item(logger_config_t * config, int num, uint8_t ublox_hw) {
    assert(config);
    if(num>=2) return 0;
    const char *name = config_fw_update_items[num];
    xSemaphoreTake(c_sem_lock, portMAX_DELAY);
    uint16_t val = config->fwupdate.channel;
    if(!strcmp(name, "update_channel")) {
        if(config->fwupdate.channel == 1) config->fwupdate.channel = 0;
        else config->fwupdate.channel++;
    }
    else if(!strcmp(name, "update_enabled")) {
        config->fwupdate.update_enabled = config->fwupdate.update_enabled ? 0 : 1;
    }
    config_save_json(config, ublox_hw);
    xSemaphoreGive(c_sem_lock);
    return 1;
}

logger_config_item_t * get_stat_screen_cfg_item(const logger_config_t *config, int num, logger_config_item_t *item) {
    assert(config);
    if(!item) return 0;
    if(num>=0 && num<lengthof(config_stat_screen_items)) {
        item->name = config_stat_screen_items[num];
        item->pos = num;
        item->value = (config->screen.stat_screens & (1 << num)) ? 1 : 0;
        item->desc = item->value ? "on" : "off";
    }
    return item;
}

int set_stat_screen_cfg_item(logger_config_t * config, int num, uint8_t ublox_hw) {
    assert(config);
    if(num>=L_CONFIG_STAT_FIELDS) return 0;
    //const char *name = config_gps_items[num];
    xSemaphoreTake(c_sem_lock, portMAX_DELAY);
    uint16_t val = config->screen.stat_screens;
    ESP_LOGI(TAG, "[%s]: %d stat_screens:%hu", __func__, num, val);
    if(num>=0 && num<lengthof(config_stat_screen_items)) {
        val ^= (1 << num);
    }
    ESP_LOGI(TAG, "[%s] set stat_screens:%hu", __func__, val);
    if(val!=config->screen.stat_screens) {
        config->screen.stat_screens = val;
        config_save_json(config, ublox_hw);
    }
    xSemaphoreGive(c_sem_lock);
    return 1;
}
logger_config_item_t * get_screen_cfg_item(const logger_config_t *config, int num, logger_config_item_t *item) {
    assert(config);
    if(!item) return 0;
    item->name = config_screen_items[num];
    item->pos = num;
    if(!strcmp(item->name, "speed_field")) {
        item->value = config->screen.speed_field;
        if(config->screen.speed_field > 0 && config->screen.speed_field <= lengthof(config_speed_field_items))
            item->desc = config_speed_field_items[config->screen.speed_field-1];
        else
            item->desc = not_set;
    } else if(!strcmp(item->name, "stat_screens_time")) {
        item->value = config->screen.stat_screens_time;
        if(item->value <= 1)
            item->desc = "1 sec";
        else if(item->value == 2)
            item->desc = "2 sec";
        else if(item->value == 3)
            item->desc = "3 sec";
        else if(item->value == 4)
            item->desc = "4 sec";
        else if(item->value >= 5)
            item->desc = "5 sec";
    } else if(!strcmp(item->name, "stat_screens")) {
        item->value = config->screen.stat_screens;
        item->desc = "menu";
    } else if(!strcmp(item->name, "screen_move_offset")) {
        item->value = config->screen_move_offset ? 1 : 0;
        item->desc = config->screen_move_offset ? "on" : "off";
    } else if(!strcmp(item->name, "board_logo")) {
        item->value = config->screen.board_logo;
        if(config->screen.board_logo > 0 && config->screen.board_logo <= lengthof(board_logos))
            item->desc = board_logos[config->screen.board_logo-1];
        else
            item->desc = not_set;
    } else if(!strcmp(item->name, "sail_logo")) {
        item->value = config->screen.sail_logo;
        if(config->screen.sail_logo > 0 && config->screen.sail_logo <= lengthof(sail_logos))
            item->desc = sail_logos[config->screen.sail_logo-1];
        else
            item->desc = not_set;
    }
    else if(!strcmp(item->name, "screen_rotation")) {
        item->value = config->screen.screen_rotation;
        if(config->screen.screen_rotation <= 3)
            item->desc = screen_rotations[config->screen.screen_rotation];
        else
            item->desc = not_set;
    }
    return item;
}

int set_screen_cfg_item(logger_config_t * config, int num, uint8_t ublox_hw) {
    assert(config);
    if(num>=L_CONFIG_SCREEN_FIELDS) return 0;
    const char *name = config_screen_items[num];
    xSemaphoreTake(c_sem_lock, portMAX_DELAY);
    if(!strcmp(name, "speed_field")) {
        if(config->screen.speed_field == 9) config->screen.speed_field = 1;
        else config->screen.speed_field++;
    } else if(!strcmp(name, "stat_screens_time")) {
        if(config->screen.stat_screens_time == 5) config->screen.stat_screens_time = 4;
        else if(config->screen.stat_screens_time == 4) config->screen.stat_screens_time = 3;
        else if(config->screen.stat_screens_time == 3) config->screen.stat_screens_time = 2;
        else if(config->screen.stat_screens_time == 2) config->screen.stat_screens_time = 1;
        else config->screen.stat_screens_time = 5;
    } else if (!strcmp(name, "screen_move_offset")) {
        config->screen_move_offset = config->screen_move_offset ? 0 : 1;
    } else if(!strcmp(name, "board_logo")) {
        if(config->screen.board_logo >= 11) config->screen.board_logo = 1;
        else config->screen.board_logo++;
    } else if(!strcmp(name, "sail_logo")) {
        if(config->screen.sail_logo >= 12) config->screen.sail_logo = 1;
        else config->screen.sail_logo++;
    }
    else if(!strcmp(name, "screen_rotation")) {
        if(config->screen.screen_rotation >= 3) {
            config->screen.screen_rotation = 0;
        }
        else {
            config->screen.screen_rotation++;
        }
    }
    config_save_json(config, ublox_hw);
    xSemaphoreGive(c_sem_lock);
    return 1;
}

logger_config_item_t * get_gps_cfg_item(const logger_config_t *config, int num, logger_config_item_t *item) {
    assert(config);
    if(!item) return 0;
    item->name = config_gps_items[num];
    item->pos = num;
    if (!strcmp(item->name, "gnss")) {
        item->value = config->gps.gnss;
        if(config->gps.gnss == 111) {
            item->desc = "G + E + B + R";
        }
        else if(config->gps.gnss == 107) {
            item->desc = "G + B + R";
        }
        else if(config->gps.gnss == 103) {
            item->desc = "G + E + R";
        }
        else if(config->gps.gnss == 47) {
            item->desc = "G + E + B";
        }
        else if(config->gps.gnss == 99) {
            item->desc = "G + R";
        }
        else if(config->gps.gnss == 43) {
            item->desc = "G + B";
        }
        else if(config->gps.gnss == 39) {
            item->desc = "G + E";
        }
        else {
            item->desc = not_set;
        }
    } else if (!strcmp(item->name, "sample_rate")) {
        item->value = config->gps.sample_rate;
        if(config->gps.sample_rate == 1) {
            item->desc = sample_rates[0];
        }
        else if(config->gps.sample_rate == 16) {
            item->desc = sample_rates[3];
        }
        else if(config->gps.sample_rate%5 == 0 && config->gps.sample_rate <= 20) {
            item->desc = sample_rates[config->gps.sample_rate/5];
        }
        else {
            item->desc = not_set;
        }
    } else if (!strcmp(item->name, "timezone")) {
        item->value = config->timezone;
        if(config->timezone == 1) {
            item->desc = "UTC+1";
        }
        else if(config->timezone == 2) {
            item->desc = "UTC+2";
        }
        else if(config->timezone == 3) {
            item->desc = "UTC+3";
        }
        else {
            item->desc = "UTC";
        }
    } else if (!strcmp(item->name, "speed_unit")) {
        item->value = config->gps.speed_unit;
        item->desc = speed_units[config->gps.speed_unit];
    } else if (!strcmp(item->name, "log_txt")) {
        item->value = config->gps.log_txt ? 1 : 0;
        item->desc = config->gps.log_txt ? "on" : "off";
    } else if (!strcmp(item->name, "log_ubx")) {
        item->value = config->gps.log_ubx ? 1 : 0;
        item->desc = config->gps.log_ubx ? "on" : "off";
    } else if (!strcmp(item->name, "log_sbp")) {
        item->value = config->gps.log_sbp ? 1 : 0;
        item->desc = config->gps.log_sbp ? "on" : "off";
    } else if (!strcmp(item->name, "log_gpy")) {
        item->value = config->gps.log_gpy ? 1 : 0;
        item->desc = config->gps.log_gpy ? "on" : "off";
    } else if (!strcmp(item->name, "log_gpx")) {
        item->value = config->gps.log_gpx ? 1 : 0;
        item->desc = config->gps.log_gpx ? "on" : "off";
    } else if (!strcmp(item->name, "log_ubx_nav_sat")) {
        item->value = config->gps.log_ubx_nav_sat ? 1 : 0;
        item->desc = config->gps.log_ubx_nav_sat ? "on" : "off";
    } else if (!strcmp(item->name, "dynamic_model")) {
        item->value = config->gps.dynamic_model;
        if(config->gps.dynamic_model == 1) {
            item->desc = "sea";
        }
        else if(config->gps.dynamic_model == 2) {
            item->desc = "automotive";
        }
        else {
            item->desc = "portable";
        }
    }
    return item;
}

int set_gps_cfg_item(logger_config_t *config, int num, uint8_t ublox_hw) {
    assert(config);
    if(num>=L_CONFIG_GPS_FIELDS) return 0;
    const char *name = config_gps_items[num];
    xSemaphoreTake(c_sem_lock, portMAX_DELAY);
    if (!strcmp(name, "gnss")) {
        if(config->gps.gnss == 111) config->gps.gnss = 107;
        else if(config->gps.gnss == 107) config->gps.gnss = 103;
        else if(config->gps.gnss == 103) config->gps.gnss = 47;
        else if(config->gps.gnss == 47) config->gps.gnss = 99;
        else if(config->gps.gnss == 99) config->gps.gnss = 43;
        else if(config->gps.gnss == 43) config->gps.gnss = 39;
        else if(config->gps.gnss == 39) config->gps.gnss = 111;
        else config->gps.gnss = 111;
    } else if (!strcmp(name, "sample_rate")) {
        if(config->gps.sample_rate == 5) config->gps.sample_rate = 1;
        else if(config->gps.sample_rate == 10) config->gps.sample_rate = 5;
        else if(config->gps.sample_rate == 16) config->gps.sample_rate = 10;
        else if(config->gps.sample_rate == 20) config->gps.sample_rate = 16;
        else config->gps.sample_rate = 20;
    } else if (!strcmp(name, "timezone")) {
        if(config->timezone == 1) config->timezone = 2;
        else if(config->timezone == 2) config->timezone = 3;
        else if(config->timezone == 3) config->timezone = 1;
        else config->timezone = 1;
   } else if (!strcmp(name, "speed_unit")) {
        if(config->gps.speed_unit == 1) config->gps.speed_unit = 0;
        else if(config->gps.speed_unit == 2) config->gps.speed_unit = 1;
        else  config->gps.speed_unit = 2;
    } else if (!strcmp(name, "log_txt")) {
        config->gps.log_txt = config->gps.log_txt ? 0 : 1;
    } else if (!strcmp(name, "log_ubx")) {
        config->gps.log_ubx = config->gps.log_ubx ? 0 : 1;
    } else if (!strcmp(name, "log_sbp")) {
        config->gps.log_sbp = config->gps.log_sbp ? 0 : 1;
    } else if (!strcmp(name, "log_gpy")) {
        config->gps.log_gpy = config->gps.log_gpy ? 0 : 1;
    } else if (!strcmp(name, "log_gpx")) {
        config->gps.log_gpx = config->gps.log_gpx ? 0 : 1;
    } else if (!strcmp(name, "log_ubx_nav_sat")) {
        config->gps.log_ubx_nav_sat = config->gps.log_ubx_nav_sat ? 0 : 1;
    } else if (!strcmp(name, "dynamic_model")) {
        if(config->gps.dynamic_model == 0) config->gps.dynamic_model = 2;
        else if(config->gps.dynamic_model == 2) config->gps.dynamic_model = 1;
        else config->gps.dynamic_model = 0;
    }
    config_save_json(config, ublox_hw);
    xSemaphoreGive(c_sem_lock);
    return 1;
}

logger_config_t *config_new() {
    logger_config_t * c = calloc(1, sizeof(logger_config_t));
    return config_init(c);
}

esp_err_t config_set_screen_cb(logger_config_t * config, void(*cb)(const char *)) {
    if(!config) return ESP_ERR_INVALID_ARG;
    config->config_changed_screen_cb = cb;
    return ESP_OK;
}

void config_delete(logger_config_t *config) {
    free(config);
}

logger_config_t *config_init(logger_config_t *config) {
    logger_config_t cf = LOGGER_CONFIG_DEFAULTS();
    memcpy(config, &cf, sizeof(logger_config_t));
    if(!c_sem_lock)
        c_sem_lock = xSemaphoreCreateRecursiveMutex();
    if(sdcard_is_mounted()) {
        config_file_path = CONFIG_SD_MOUNT_POINT"/"CFG_FILE_NAME;
        config_file_backup_path = CONFIG_SD_MOUNT_POINT"/"CFG_FILE_NAME_BACKUP;
        config_file_default_path = CONFIG_SD_MOUNT_POINT"/"CFG_FILE_NAME_DEFAULT;
    } else 
#if defined(CONFIG_USE_FATFS)
    if(fatfs_is_mounted()) { // first choice is internal fat partition
        config_file_path = CONFIG_FATFS_MOUNT_POINT"/"CFG_FILE_NAME;
        config_file_backup_path = CONFIG_FATFS_MOUNT_POINT"/"CFG_FILE_NAME_BACKUP;
        config_file_default_path = CONFIG_FATFS_MOUNT_POINT"/"CFG_FILE_NAME_DEFAULT;
    } else 
#endif
#if defined(CONFIG_USE_LITTLEFS)
    if(littlefs_is_mounted()) {
        config_file_path = CONFIG_LITTLEFS_MOUNT_POINT"/"CFG_FILE_NAME;
        config_file_backup_path = CONFIG_LITTLEFS_MOUNT_POINT"/"CFG_FILE_NAME_BACKUP;
        config_file_default_path = CONFIG_LITTLEFS_MOUNT_POINT"/"CFG_FILE_NAME_DEFAULT;
    } else 
#endif
    {
        ESP_LOGE(TAG, "No filesystem mounted");
        return 0;
    }
    esp_event_post(CONFIG_EVENT, LOGGER_CONFIG_EVENT_CONFIG_INIT_DONE, config, sizeof(logger_config_t), portMAX_DELAY);
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

JsonNode *config_parse(const char *json) {
    ILOG(TAG,"[%s]",__func__);
    JsonNode *root = 0;
    if (!json_validate(json)) {
        ESP_LOGE(TAG, "Bad json: %s", json);
        return 0;
    }
    root = json_decode(json);
    if (!root) {
        ESP_LOGE(TAG, "Parser error: %s", json);
        return 0;
    }
    return root;
}

int config_set(logger_config_t *config, JsonNode *root, const char *str, uint8_t force) {
#if (CONFIG_LOGGER_CONFIG_LOG_LEVEL < 2)
    ILOG(TAG,"[%s] name: %s",__func__, str ? str : "-");
#endif
    if (!root) {
        return -1;
    }
    int changed = 0;
    JsonNode *name = 0, *value = 0;
    const char *var = 0;
    if (!str) {
        name = json_find_member(root, "name");
        value = json_find_member(root, "value");
        if (name && name->tag == JSON_STRING)
            var = name->data.string_;
    } else {
        // name = str;
        value = json_find_member(root, str);
        // value = name;
        var = str;
    }
    if (!var) {
#if CONFIG_LOGGER_CONFIG_LOG_LEVEL < 3
        printf("[%s] ! var\n", __FUNCTION__);
#endif
        goto err;
    }
    if (!strstr(config_item_names, var) && !strstr(config_item_names_compat, var)) {
#if CONFIG_LOGGER_CONFIG_LOG_LEVEL < 3
        printf("[%s] ! in names\n", __FUNCTION__);
#endif
        goto err;
    }
    if (!value) {
#if CONFIG_LOGGER_CONFIG_LOG_LEVEL < 3
        printf("[%s] ! value\n", __FUNCTION__);
#endif
        goto err;
    }
    if (name || str)
        DLOG(TAG, "[%s] {name: ( %s | %s )}\n", __FUNCTION__, (name && name->data.string_ ? name->data.string_ : "-"), (str ? str : "-"));
    if (value)
        DLOG(TAG, "[%s] {value: ( %s | %f ), key: %s}\n", __FUNCTION__, (value->tag == JSON_STRING ? value->data.string_ : "-"), (value->tag == JSON_NUMBER ? value->data.number_ : 0), (value->key ? value->key : "-"));

#ifdef USE_CUSTOM_CALIBRATION_VAL
    if (!strcmp(var, "cal_bat")) {  // calibration for read out bat voltage
        if (!value || value->tag != JSON_NUMBER) {
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

    } else 
#endif
    if (!strcmp(var, "speed_unit")) {  // conversion m/s to km/h, for knots use 1.944
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        float val = value->data.number_;
        if (force || val != config->gps.speed_unit) {
            config->gps.speed_unit = val;
            changed = 1;
        }

    } else if (!strcmp(var, "sample_rate")) {  // gps_rate in Hz, 1, 5 or 10Hz !!!
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        uint8_t val = value->data.number_;
        if (force || val != config->gps.sample_rate) {
            config->gps.sample_rate = val;
            changed = 1;
        }

    } else if (!strcmp(var, "gnss")) {  // default setting 2 GNSS, GPS & GLONAS
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        uint8_t val = value->data.number_;
        if (force || val != config->gps.gnss) {
            config->gps.gnss = val;
            changed = 1;
        }

    } else if (!strcmp(var, "speed_field")) {  // choice for first field in speed screen !!!
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        uint8_t val = value->data.number_;
        if (force || val != config->screen.speed_field) {
            config->screen.speed_field = val;
            changed = 1;
        }

    }

    else if (!strcmp(var, "speed_large_font")) {  // fonts on the first line are bigger, actual speed
        // font is smaller
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        uint8_t val = value->data.number_;
        if (force || val != config->screen.speed_large_font) {
            config->screen.speed_large_font = val;
            changed = 1;
        }

    } else if (!strcmp(var, "dynamic_model")) {  // choice for dynamic model "Sea",if 0 model "portable"
        // is used !!
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        uint8_t val = value->data.number_;
        if (force || val != config->gps.dynamic_model) {
            config->gps.dynamic_model = val;
            changed = 1;
        }

    } else if (!strcmp(var, "timezone")) {  // choice for timedifference in hours with UTC, for Belgium
        // 1 or 2 (summertime)
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        float val = value->data.number_;
        if (force || val != config->timezone) {
            config->timezone = val;
            changed = 1;
        }

    } else if (!strcmp(var, "stat_screens") || !strcmp(var, "Stat_screens")) {  // choice for stats field when no speed, here stat_screen
        // 1, 2 and 3 will be active
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        uint32_t val = value->data.number_;
        if (force || val != config->screen.stat_screens) {
            config->screen.stat_screens = val;
            changed = 1;
        }

    } else if (!strcmp(var, "stat_screens_time") || !strcmp(var, "Stat_screens_time")) {  // time between switching stat_screens
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        uint8_t val = value->data.number_;
        if (force || val != config->screen.stat_screens_time) {
            config->screen.stat_screens_time = val;
            changed = 1;
        }

    } else if (!strcmp(var, "gpio12_screens") || !strcmp(var, "GPIO12_screens")) {  // choice for stats field when gpio12 is activated
        // (pull-up high, low = active)
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        uint32_t val = value->data.number_;
        if (force || val != config->screen.gpio12_screens) {
            config->screen.gpio12_screens = val;
            changed = 1;
        }

    // } else if (!strcmp(var, "stat_screens_persist")) {  // choice for stats field when no speed, here
    //     // stat_screen 1, 2 and 3 will be active / for
    //     // resave the config
    //     if (!value || value->tag != JSON_NUMBER) {
    //         goto err;
    //     }
    //     uint8_t val = value->data.number_;
    //     if (force || val != config->screen.stat_screens_persist) {
    //         config->screen.stat_screens_persist = val;
    //         changed = 1;
    //     }

    // } else if (!strcmp(var, "gpio12_screens_persist")) {  // choice for stats field when gpio12 is
    //     // activated (pull-up high, low = active) / for
    //     // resave the config
    //     if (!value || value->tag != JSON_NUMBER) {
    //         goto err;
    //     }
    //     uint8_t val = value->data.number_;
    //     if (force || val != config->screen.gpio12_screens_persist) {
    //         config->screen.gpio12_screens_persist = val;
    //         changed = 1;
    //     }

    } else if (!strcmp(var, "screen_move_offset")) {
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        uint8_t val = value->data.number_;
        if (force || val != config->screen_move_offset) {
            config->screen_move_offset = val;
            changed = 1;
        }

    }  // log to .gpx
    else if (!strcmp(var, "file_date_time")) {
    } else if (!strcmp(var, "board_logo") || !strcmp(var, "board_Logo") || !strcmp(var, "Board_Logo")) {
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        uint8_t val = value->data.number_;
        if (force || val != config->screen.board_logo) {
            config->screen.board_logo = val;
            changed = 1;
        }

    } else if (!strcmp(var, "sail_logo") || !strcmp(var, "sail_Logo") || !strcmp(var, "Sail_Logo")) {
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        uint8_t val = value->data.number_;
        if (force || val != config->screen.sail_logo) {
            config->screen.sail_logo = val;
            changed = 1;
        }

    } else if (!strcmp(var, "stat_speed")) {  // max speed in m/s for showing Stat screens
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        uint8_t val = value->data.number_;
        if (force || val != config->screen.stat_speed) {
            config->screen.stat_speed = val;
            changed = 1;
        }

    } else if (!strcmp(var, "bar_length")) {  // choice for bar indicator for length of run in m
        // (nautical mile)
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        uint16_t val = value->data.number_;
        if (force || val != config->gps.bar_length) {
            config->gps.bar_length = val;
            changed = 1;
        }

    } else if (!strcmp(var, "archive_days")) {  // how many days files will be moved to the "Archive" dir
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        uint16_t val = value->data.number_;
        if (force || val != config->archive_days) {
            config->archive_days = val;
            changed = 1;
        }

    } else if (!strcmp(var, "update_enabled")) {
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        uint8_t val = value->data.number_;
        if (force || val != config->fwupdate.update_enabled) {
            config->fwupdate.update_enabled = val;
            changed = 1;
        }
    } else if (!strcmp(var, "update_channel")) {
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        uint8_t val = value->data.number_;
        if (force || val != config->fwupdate.channel) {
            config->fwupdate.channel = val;
            changed = 1;
        }
    } else if (!strcmp(var, "log_txt") || !strcmp(var, "logTXT")) {  // switchinf off .txt files
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        uint8_t val = value->data.number_;
        if (force || val != config->gps.log_txt) {
            config->gps.log_txt = val;
            changed = 1;
        }
    } else if (!strcmp(var, "log_ubx") || !strcmp(var, "logUBX")) {  // log to .ubx
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        uint8_t val = value->data.number_;
        if (force || val != config->gps.log_ubx) {
            config->gps.log_ubx = val;
            changed = 1;
        }

    } else if (!strcmp(var, "log_ubx_nav_sat") || !strcmp(var, "logUBX_nav_sat")) {  // log nav sat msg to .ubx
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        uint8_t val = value->data.number_;
        if (force || val != config->gps.log_ubx_nav_sat) {
            config->gps.log_ubx_nav_sat = val;
            changed = 1;
        }

    } else if (!strcmp(var, "log_sbp") || !strcmp(var, "logSBP")) {  // log to .sbp
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        uint8_t val = value->data.number_;
        if (force || val != config->gps.log_sbp) {
            config->gps.log_sbp = val;
            changed = 1;
        }

    } else if (!strcmp(var, "log_gpy") || !strcmp(var, "logGPY")) {
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        uint8_t val = value->data.number_;
        if (force || val != config->gps.log_gpy) {
            config->gps.log_gpy = val;
            changed = 1;
        }

    }  // log to .gps
    else if (!strcmp(var, "log_gpx") || !strcmp(var, "logGPX")) {
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        uint8_t val = value->data.number_;
        if (force || val != config->gps.log_gpx) {
            config->gps.log_gpx = val;
            changed = 1;
        }

    }  // log to .gpx
    else if (!strcmp(var, "file_date_time")) {
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        uint8_t val = value->data.number_;
        if (force || val != config->file_date_time) {
            config->file_date_time = val;
            changed = 1;
        }

    }
    else if (!strcmp(var, "screen_rotation")) {
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        uint8_t val = value->data.number_;
        if (force || val != config->screen.screen_rotation) {
            config->screen.screen_rotation = val;
            changed = 1;
        }

    }  // type of filenaming, with MAC adress or datetime
    else if (!strcmp(var, "ubx_file") || !strcmp(var, "UBXfile")) {
        if (!value || value->tag != JSON_STRING) {
            goto err;
        }
        const char *val = value->data.string_;
        if (force || strcmp(val, config->ubx_file)) {
            size_t len = strlen(val);
            memcpy(config->ubx_file, val, len);
            config->ubx_file[len] = 0;
            changed = 1;
        }
    }  // your preferred filename
    else if (!strcmp(var, "sleep_info") || !strcmp(var, "Sleep_info")) {
        if (!value || value->tag != JSON_STRING) {
            goto err;
        }
        const char *val = value->data.string_;
        if (force || strcmp(val, config->sleep_info)) {
            size_t len = strlen(val);
            memcpy(config->sleep_info, val, len);
            config->sleep_info[len] = 0;
            changed = 1;
        }
    }  // your preferred sleep text
    else if (strstr(var, "ssid")==var) {
        if (!value || value->tag != JSON_STRING) {
            goto err;
        }
        const char *val = value->data.string_;
        uint8_t num = 0;
        if (var[4] == 49) num = 1;
        else if(var[4] == 50) num = 2;
        else if(var[4] == 51) num = 3;
        else if(var[4] == 52) num = 4;
        if (force || strcmp(val, config->wifi_sta[num].ssid)) {
            size_t len = strlen(val);
            memcpy(config->wifi_sta[num].ssid, val, len);
            config->wifi_sta[num].ssid[len] = 0;
            changed = 1;
        }
    }  // your SSID
    else if (strstr(var, "password")==var) {
        if (!value || value->tag != JSON_STRING) {
            goto err;
        }
        const char *val = value->data.string_;
        uint8_t num = 0;
        if (var[8] == 49) num = 1;
        else if(var[8] == 50) num = 2;
        else if(var[8] == 51) num = 3;
        else if(var[8] == 52) num = 4;
        if (force || strcmp(val, config->wifi_sta[num].password)) {
            size_t len = strlen(val);
            memcpy(config->wifi_sta[num].password, val, len);
            config->wifi_sta[num].password[len] = 0;
            changed = 1;
        }
    }  // your password
    else if (!strcmp(var, "hostname")) {
        if (!value || value->tag != JSON_STRING) {
            goto err;
        }
        const char *val = value->data.string_;
        if (force || strcmp(val, config->hostname)) {
            size_t len = strlen(val);
            memcpy(config->hostname, val, len);
            config->hostname[len] = 0;
            changed = 1;
        }
    }  // your hostname
    else {
    err:
        ESP_LOGW(TAG, "[%s] error: %s %d", __FUNCTION__, var ? var : "-", value ? value->tag : -1);
        /* if(root) {
            char * x = json_stringify(root, "\n");
            printf("%s", x);
            free(x);
        } */
        changed = -2;
    }
    if (config->config_changed_screen_cb && changed>0)
        config->config_changed_screen_cb(var);
    return changed;
}

int config_set_var(logger_config_t *config, const char *json, const char *var) {
#if (CONFIG_LOGGER_CONFIG_LOG_LEVEL < 2)
    ILOG(TAG, "[%s] '%s'", __FUNCTION__, json ? json : var ? var : "-");
#endif
    JsonNode *root = config_parse(json);
    if (!root) {
        return -1;
    }
    int ret = config_set(config, root, var, 0);
    if (root)
        json_delete(root);
    return ret;
}

int config_save_var(struct logger_config_s *config, const char *json, const char *var, uint8_t ublox_hw) {
    ILOG(TAG,"[%s]",__func__);
    IMEAS_START();
    xSemaphoreTake(c_sem_lock, portMAX_DELAY);
    int ret = config_set_var(config, json, var);
    if (ret > 0) {
        ret = config_save_json(config, ublox_hw);
    }
    xSemaphoreGive(c_sem_lock);
    IMEAS_END(TAG, "[%s] took %llu us", __FUNCTION__);
    return ret;
}

int config_save_var_b(logger_config_t *config, const char *json, uint8_t ublox_hw) {
    ILOG(TAG,"[%s]",__func__);
    return config_save_var(config, json, 0, ublox_hw);
}

esp_err_t config_decode(logger_config_t *config, const char *json) {
    ILOG(TAG,"[%s]",__func__);
    int ret = ESP_OK;
    JsonNode *root = config_parse(json);
    if (!root) {
        return ESP_FAIL;
    }
#define SET_CONF(a, b) config_set(config, a, b, 0);
    int changed;
    //changed = SET_CONF(root, "cal_bat");
    changed = SET_CONF(root, "speed_unit");
    changed = SET_CONF(root, "sample_rate");
    changed = SET_CONF(root, "gnss");
    changed = SET_CONF(root, "speed_field");
    changed = SET_CONF(root, "speed_large_font");
    changed = SET_CONF(root, "dynamic_model");
    changed = SET_CONF(root, "bar_length");
    changed = SET_CONF(root, "stat_screens");
    if (changed <= -2)
        changed = SET_CONF(root, "Stat_screens");
    changed = SET_CONF(root, "stat_screens_time");
    if (changed <= -2)
        changed = SET_CONF(root, "Stat_screens_time");
    changed = SET_CONF(root, "stat_speed");
    changed = SET_CONF(root, "archive_days");
    changed = SET_CONF(root, "gpio12_screens");
    if (changed <= -2)
        changed = SET_CONF(root, "GPIO12_screens");
    changed = SET_CONF(root, "screen_move_offset");
    changed = SET_CONF(root, "board_logo");
    if (changed <= -2)
        changed = SET_CONF(root, "board_Logo");
    if (changed <= -2)
        changed = SET_CONF(root, "Board_Logo");
    changed = SET_CONF(root, "sail_logo");
    if (changed <= -2)
        changed = SET_CONF(root, "sail_Logo");
    if (changed <= -2)
        changed = SET_CONF(root, "Sail_Logo");
    changed = SET_CONF(root, "log_txt");
    if (changed <= -2)
        changed = SET_CONF(root, "logTXT");
    changed = SET_CONF(root, "log_ubx");
    if (changed <= -2)
        changed = SET_CONF(root, "logUBX");
    changed = SET_CONF(root, "log_ubx_nav_sat");
    if (changed <= -2)
        changed = SET_CONF(root, "logUBX_nav_sat");
    changed = SET_CONF(root, "log_sbp");
    if (changed <= -2)
        changed = SET_CONF(root, "logSBP");
    changed = SET_CONF(root, "log_gpy");
    if (changed <= -2)
        changed = SET_CONF(root, "logGPY");
    changed = SET_CONF(root, "log_gpx");
    if (changed <= -2)
        changed = SET_CONF(root, "logGPX");
    changed = SET_CONF(root, "file_date_time");
    changed = SET_CONF(root, "screen_rotation");
    changed = SET_CONF(root, "update_enabled");
    changed = SET_CONF(root, "update_channel");
    changed = SET_CONF(root, "timezone");
    changed = SET_CONF(root, "ubx_file");
    if (changed <= -2)
        changed = SET_CONF(root, "UBXfile");
    changed = SET_CONF(root, "sleep_info");
    if (changed <= -2)
        changed = SET_CONF(root, "Sleep_info");
    for(uint8_t i=0,j=L_CONFIG_SSID_MAX; i<j; i++) {
        char ssid[12]={0}, password[12]={0};
        memcpy(&ssid[0], "ssid", 4);
        memcpy(&password[0], "password", 8);
        if(i>0) ssid[4] = password[8] = i+48;
        SET_CONF(root, &ssid[0]);
        SET_CONF(root, &password[0]);
    }
    changed = SET_CONF(root, "hostname");
    
    if (root)
        json_delete(root);
    return ret;
#undef SET_CONF
}

esp_err_t config_load_json(logger_config_t *config) {
    ILOG(TAG,"[%s]",__func__);
    IMEAS_START();
    int ret = ESP_OK;
    char *json = 0;
    xSemaphoreTake(c_sem_lock, portMAX_DELAY);
    if ((json = s_read_from_file(config_file_path, 0))) {
        ILOG(TAG,"[%s] from %s done",__func__, config_file_path);
    } else if ((json = s_read_from_file(config_file_backup_path, 0))) {
        ILOG(TAG,"[%s] from %s done",__func__, config_file_backup_path);
    } else {
        ESP_LOGE(TAG, "configuration not found...");
        goto done;
    }
    //printf("conf:%s\n", json);
    ret = config_decode(config, json);
done:
    xSemaphoreGive(c_sem_lock);
    if (json)
        free(json);
    esp_event_post(CONFIG_EVENT, LOGGER_CONFIG_EVENT_CONFIG_LOAD_DONE, config, sizeof(logger_config_t), portMAX_DELAY);
    IMEAS_END(TAG, "[%s] took %llu us", __FUNCTION__);
    return ret;
}

esp_err_t config_save_json(logger_config_t *config, uint8_t ublox_hw) {
    ILOG(TAG,"[%s]",__func__);
    int ret = ESP_OK;
    strbf_t sb;
    strbf_init(&sb);
    char *json = config_encode_json(config, &sb, ublox_hw);
    if (!json_validate(json)) {
        ESP_LOGE(TAG, "[%s] bad json: %s", __FUNCTION__ , json);
        goto done;
    }
#if (CONFIG_LOGGER_CONFIG_LOG_LEVEL <= 1)
    printf("[%s] save json: %s", __FUNCTION__, json);
#endif
    s_rename_file_n(config_file_path, config_file_backup_path, 1);
    ret = s_write(config_file_path, 0, sb.start, sb.cur - sb.start);
done:
    strbf_free(&sb);
    esp_event_post(CONFIG_EVENT, !ret ? LOGGER_CONFIG_EVENT_CONFIG_SAVE_DONE : LOGGER_CONFIG_EVENT_CONFIG_SAVE_FAIL, config, sizeof(logger_config_t), portMAX_DELAY);
    return ret;
}

logger_config_t *config_fix_values(logger_config_t *config) {
    ILOG(TAG,"[%s]",__func__);
    // int Logo_choice=config->Logo_choice;//preserve value config->Logo_choice
    // for config->txt update !!
    /* if (config->cal_bat > 1)
        config->cal_bat = 1.0; */
    if (config->file_date_time == 0)
        config->gps.log_txt = 1;  // because txt file is needed for generating new file count !!
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
        if (orig->gps.speed_unit != config->gps.speed_unit)
            return 2;
        if (orig->gps.sample_rate != config->gps.sample_rate)
            return 3;
        if (orig->gps.gnss != config->gps.gnss)
            return 4;
        if (orig->screen.speed_field != config->screen.speed_field)
            return 5;
        if (orig->screen.speed_large_font != config->screen.speed_large_font)
            return 6;
        if (orig->gps.bar_length != config->gps.bar_length)
            return 7;
        if (orig->screen.stat_speed != config->screen.stat_speed)
            return 8;
        if (orig->archive_days != config->archive_days)
            return 9;
        if (orig->screen.stat_screens_time != config->screen.stat_screens_time)
            return 10;
        if (orig->screen.stat_screens != config->screen.stat_screens)
            return 11;
        // if (orig->screen.stat_screens_persist != config->screen.stat_screens_persist)
        //     return 12;
        if (orig->screen.gpio12_screens != config->screen.gpio12_screens)
            return 13;
        if (orig->screen_move_offset != config->screen_move_offset)
            return 14;
        if (orig->screen.board_logo != config->screen.board_logo)
            return 15;
        if (orig->screen.sail_logo != config->screen.sail_logo)
            return 16;
        if (orig->gps.log_txt != config->gps.log_txt)
            return 18;
        if (orig->gps.log_ubx != config->gps.log_ubx)
            return 19;
        if (orig->gps.log_ubx_nav_sat != config->gps.log_ubx_nav_sat)
            return 2;
        if (orig->gps.log_sbp != config->gps.log_sbp)
            return 21;
        if (orig->gps.log_gpy != config->gps.log_gpy)
            return 22;
        if (orig->file_date_time != config->file_date_time)
            return 23;
        if (orig->gps.dynamic_model != config->gps.dynamic_model)
            return 24;
        if (orig->timezone != config->timezone)
            return 25;
        if (strcmp(config->ubx_file, orig->ubx_file))
            return 26;
        if (strcmp(config->sleep_info, orig->sleep_info))
            return 27;
        if (orig->speed_field_count != config->speed_field_count)
            return 30;
        if (strcmp(config->hostname, orig->hostname))
            return 32;
        if (config->screen.screen_rotation != orig->screen.screen_rotation)
            return 33;
        if (config->fwupdate.update_enabled != orig->fwupdate.update_enabled)
            return 34;
        if (config->fwupdate.channel != orig->fwupdate.channel)
            return 35;
        for(uint8_t i=0,j=L_CONFIG_SSID_MAX,k=34; i<j; i++,k++) {
            if (strcmp(config->wifi_sta[i].ssid, orig->wifi_sta[i].ssid))
                return k;
            if (strcmp(config->wifi_sta[i].password, orig->wifi_sta[i].password))
                return k+5;
        }
    }
    return 0;
}

char *config_get(const logger_config_t *config, const char *name, char *str, size_t *len, size_t max, uint8_t mode, const uint8_t ublox_hw) {
    ILOG(TAG, "[%s] %s", __FUNCTION__, name);
    *len = 0;
    if (!config) {
        return 0;
    }
    // _ubx_hw_t ublox_hw = get_ublox_hw();

    if (!strstr(config_item_names, name) || (ublox_hw != UBX_TYPE_M8 && !strcmp(name, "dynamic_model"))){
        return 0;
    }

    strbf_t lsb;
    if (str)
        strbf_inits(&lsb, str, max);
    else
        strbf_init(&lsb);

    if (mode) {
        strbf_puts(&lsb, "{\"name\":");
    }

    strbf_puts(&lsb, "\"");
    strbf_puts(&lsb, name);
    strbf_puts(&lsb, "\"");

    if (mode) {
        strbf_puts(&lsb, ",\"value\"");
    }
    strbf_putc(&lsb, ':');
#ifdef USE_CUSTOM_CALIBRATION_VAL
    if (!strcmp(name, "cal_bat")) {  // calibration for read out bat voltage
        strbf_putd(&lsb, config->cal_bat, 1, 4);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"calibration for read out bat voltage\",\"type\":\"float\",\"ext\":\"V\"");
        }
    } else 
#endif
    if (!strcmp(name, "speed_unit")) {  // speed units, 0 = m/s 1 = km/h, 2 = knots
        strbf_putn(&lsb, config->gps.speed_unit);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"Speed display units\",\"type\":\"int\"");
            strbf_puts(&lsb, ",\"values\":[");
            for(uint8_t i = 0, j=(lengthof(speed_units)); i < j; i++) {
                strbf_puts(&lsb, "{\"value\":");
                strbf_putn(&lsb, i);
                strbf_puts(&lsb, ",\"title\":\"");
                strbf_puts(&lsb, speed_units[i]);
                strbf_puts(&lsb, "\"}");
                if(i < j-1) strbf_putc(&lsb, ',');
            }
            strbf_puts(&lsb, "]");
        }
    } else if (!strcmp(name, "sample_rate")) {  // gps_rate in Hz, 1, 5 or 10Hz !!!
        strbf_putn(&lsb, config->gps.sample_rate);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"gps_rate in Hz\",\"type\":\"int\"");
            strbf_puts(&lsb,",\"values\":[");
            for(uint8_t i = 0, j = 5, k; i < j; i++) {
                k = (i==3) ? (i)*5+1 : (i > 0) ? (i)*5 : 1;
                strbf_puts(&lsb, "{\"value\":");
                strbf_putn(&lsb, k);
                strbf_puts(&lsb, ",\"title\":\"");
                strbf_puts(&lsb, sample_rates[i]);
                strbf_puts(&lsb, "\"}");
                if(i < j-1) strbf_putc(&lsb, ',');
            }
            strbf_puts(&lsb, "]");
            strbf_puts(&lsb, ",\"ext\":\"Hz\"");
        }
    } else if (!strcmp(name, "gnss")) {
        strbf_putn(&lsb, config->gps.gnss);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\" default ");
            if (ublox_hw >= UBX_TYPE_M9) {
                strbf_puts(&lsb, "4 gnss: GPS(us) + GALILEO(eu) + BEIDOU(cn) + GLONASS(ru), also SBAS(us) and QZSS(jp) augmentation services enabled by default.");
            }
            else {
                strbf_puts(&lsb, "3 gnss: GPS + GALILEO + GLONASS");
            }
            strbf_puts(&lsb, "\",\"type\":\"int\",\"values\":[");
            strbf_puts(&lsb, "{\"value\":39,\"title\":\"GPS(G) + GALILEO(E)\"},");
            strbf_puts(&lsb, "{\"value\":43,\"title\":\"GPS(G) + BEIDOU(B)\"},");
            strbf_puts(&lsb, "{\"value\":99,\"title\":\"GPS(G) + GLONASS(R)\"},");
            strbf_puts(&lsb, "{\"value\":47,\"title\":\"GPS(G) + GALILEO(E) + BEIDOU(B)\"},");
            strbf_puts(&lsb, "{\"value\":103,\"title\":\"GPS(G) + GALILEO(E) + GLONASS(R)\"},");
            strbf_puts(&lsb, "{\"value\":107,\"title\":\"GPS(G) + BEIDOU(B) + GLONASS(R) \"}");
            if (ublox_hw >= UBX_TYPE_M9) {
                strbf_puts(&lsb, ",{\"value\":111,\"title\":\"GPS(G) + GALILEO(E) + BEIDOU(B) + GLONASS(R)\"}");
            }
            strbf_puts(&lsb, "]");
        }
    } else if (!strcmp(name, "speed_field")) {  // choice for first field in speed screen !!!
        strbf_putn(&lsb, config->screen.speed_field);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"choice for first field in speed screen\",\"type\":\"int\"");
            strbf_puts(&lsb, ",\"values\":[");
            for(uint8_t i = 0, j = lengthof(config_speed_field_items); i < j; i++) {
                strbf_puts(&lsb, "{\"value\":");
                strbf_putn(&lsb, i+1);
                strbf_puts(&lsb, ",\"title\":\"");
                strbf_puts(&lsb, config_speed_field_items[i]);
                strbf_puts(&lsb, "\"}");
                if(i < j-1) strbf_putc(&lsb, ',');
            }
            strbf_puts(&lsb, "]");
        }
    }

    else if (!strcmp(name, "speed_large_font")) {  // fonts on the first line are bigger, actual speed font is smaller
        strbf_putn(&lsb, config->screen.speed_large_font);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"fonts on the first line are bigger, actual speed font is smaller\",\"type\":\"bool\"");
        }
    } else if (!strcmp(name, "dynamic_model")) {  // choice for dynamic model "Sea",if 0 model "portable" is used !!
        strbf_putn(&lsb, config->gps.dynamic_model);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"choice for dynamic model 'Sea', if 0 model 'Portable' is used !!\",\"type\":\"int\",");
            strbf_puts(&lsb, "\"values\":[{\"value\":0,\"title\":\"Portable\"},");
            strbf_puts(&lsb, "{\"value\":1,\"title\":\"Sea\"},");
            strbf_puts(&lsb, "{\"value\":2,\"title\":\"Automotive\"}");
            strbf_puts(&lsb, "]");
        }

    } else if (!strcmp(name, "timezone")) {  // choice for timedifference in hours with UTC, for Belgium 1 or 2 (summertime)
        strbf_putd(&lsb, config->timezone, 1, 0);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"timezone: The local time difference in hours with UTC\",\"type\":\"float\",\"ext\":\"h\"");
            strbf_puts(&lsb, ",\"values\":[");
            for(int8_t i = 0, j=4; i < j; i++) {
                strbf_puts(&lsb, "{\"value\":");
                strbf_putn(&lsb, i);
                strbf_puts(&lsb, ",\"title\":\"GMT");
                if (i > 0) strbf_putc(&lsb, '+');
                else if (i < 0) strbf_putc(&lsb, '-');
                strbf_putn(&lsb, i);
                strbf_puts(&lsb, "\"}");
                if(i < j-1) strbf_putc(&lsb, ',');
            }
            strbf_puts(&lsb, "]");
        }                                        // 2575
    } else if (!strcmp(name, "stat_screens")||!strcmp(name, "Stat_screens")) {  // choice for stats field when no speed, here stat_screen
        // 1, 2 and 3 will be active
        strbf_putn(&lsb, config->screen.stat_screens);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"Stat_screens choice : activate / deactivate screens to show.\",\"type\":\"int\"");
            strbf_puts(&lsb, ",\"toggles\":[");
            for(uint8_t i= 0, j = 1, k = lengthof(config_stat_screen_items); i < k; i++, j <<= 1) {
                strbf_puts(&lsb, "{\"pos\":");
                strbf_putn(&lsb, i);
                strbf_puts(&lsb, ",\"title\":\"");
                strbf_puts(&lsb, config_stat_screen_items[i]);
                strbf_puts(&lsb, "\",\"value\":");
                strbf_putn(&lsb, j);
                strbf_puts(&lsb, "}");
                if(i < k-1) strbf_putc(&lsb, ',');
            }
            strbf_puts(&lsb, "]");
        }
    } else if (!strcmp(name, "stat_screens_time")||!strcmp(name, "Stat_screens_time")) {  // time between switching stat_screens
        strbf_putn(&lsb, config->screen.stat_screens_time);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"The time between toggle the different stat screens\",\"type\":\"int\"");
            strbf_puts(&lsb, ",\"values\":[");
            for(uint8_t i = 0, j = 5; i < j; i++) {
                strbf_puts(&lsb, "{\"value\":");
                strbf_putn(&lsb, i+1);
                strbf_puts(&lsb, ",\"title\":\"");
                strbf_putn(&lsb, i+1);
                strbf_puts(&lsb, " sec\"}");
                if(i < j-1) strbf_putc(&lsb, ',');
            }
            strbf_puts(&lsb, "]");
        }
    } else if (!strcmp(name, "gpio12_screens")||!strcmp(name, "GPIO12_screens")) {  // choice for stats field when gpio12 is activated
        // (pull-up high, low = active)
        strbf_putn(&lsb, config->screen.gpio12_screens);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"GPIO12_screens choice : Every digit shows the according GPIO_screen after each push. Screen 4 = s10 runs, screen 5 = alfa's.\",\"type\":\"int\"");
        }
    // } else if (!strcmp(name, "stat_screens_persist")) {  // choice for stats field when no speed, here
    //     // stat_screen 1, 2 and 3 will be active / for
    //     // resave the config
    //     strbf_putn(&lsb, config->screen.stat_screens_persist);
    //     if (mode) {
    //         strbf_puts(&lsb, ",\"info\":\"Persist stat screens\",\"type\":\"int\"");
    //     }
    // } else if (!strcmp(name, "gpio12_screens_persist")) {  // choice for stats field when gpio12 is
    //     // activated (pull-up high, low = active) / for
    //     // resave the config
    //     strbf_putn(&lsb, config->screen.gpio12_screens_persist);
    //     if (mode) {
    //         strbf_puts(&lsb, ",\"info\":\"choice for stats field when gpio12 is activated (pull-up high, low = active) / for resave the config\",\"type\":\"int\"");
    //     }
    } else if (!strcmp(name, "screen_move_offset")) {
        strbf_putn(&lsb, config->screen_move_offset);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"move epd sceen content to pervent panel burn\",\"type\":\"bool\"");
        }
    } else if (!strcmp(name, "board_logo")||!strcmp(name, "board_logo")||!strcmp(name, "Board_Logo")) {
        strbf_putn(&lsb, config->screen.board_logo);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"Board_Logo\",\"type\":\"int\"");
            strbf_puts(&lsb, ",\"values\":[");
            for(uint8_t i = 0, j = lengthof(board_logos); i < j; i++) {
                strbf_puts(&lsb, "{\"value\":");
                strbf_putn(&lsb, i+1);
                strbf_puts(&lsb, ",\"title\":\"");
                strbf_puts(&lsb, board_logos[i]);
                strbf_puts(&lsb, "\"}");
                if(i < j-1) strbf_putc(&lsb, ',');
            }
            strbf_puts(&lsb, "]");
        }
    } else if (!strcmp(name, "sail_logo")||!strcmp(name, "sail_logo")||!strcmp(name, "Sail_Logo")) {
        strbf_putn(&lsb, config->screen.sail_logo);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"Sail Logo\",\"type\":\"int\"");
            strbf_puts(&lsb, ",\"values\":[");
            for(uint8_t i = 0, j = lengthof(sail_logos); i < j; i++) {
                strbf_puts(&lsb, "{\"value\":");
                strbf_putn(&lsb, i+1);
                strbf_puts(&lsb, ",\"title\":\"");
                strbf_puts(&lsb, sail_logos[i]);
                strbf_puts(&lsb, "\"}");
                if(i < j-1) strbf_putc(&lsb, ',');
            }
            strbf_puts(&lsb, "]");
        }
    } else if (!strcmp(name, "stat_speed")) {  // max speed in m/s for showing Stat screens
        strbf_putn(&lsb, config->screen.stat_speed);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"max speed in m/s for showing Stat screens\",\"type\":\"int\",\"ext\":\"m/s\"");
        }
    } else if (!strcmp(name, "bar_length")) {  // choice for bar indicator for length of run in m
        // (nautical mile)
        strbf_putn(&lsb, config->gps.bar_length);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"bar_length: Default length = 1852 m for 100% bar (=Nautical mile)\",\"type\":\"int\",\"ext\":\"m\"");
        }
    } else if (!strcmp(name, "archive_days")) {  // how many days files will be moved to the "Archive" dir
        strbf_putn(&lsb, config->archive_days);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"how many days files will be moved to the 'Archive' dir\",\"type\":\"int\",\"ext\":\"d\"");
        }
    } else if (!strcmp(name, "update_enabled")) {  // switchinf off .txt files
        strbf_putn(&lsb, config->fwupdate.update_enabled);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"wether to allow automatic firmware updates or not\",\"type\":\"bool\"");
        }
    } else if (!strcmp(name, "update_channel")) {  // switchinf off .txt files
        strbf_putn(&lsb, config->fwupdate.channel);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"automatic firmware update channel\",\"type\":\"int\"");
            strbf_puts(&lsb, ",\"values\":[");
            for(uint8_t i = 0, j = lengthof(channels); i < j; i++) {
                strbf_puts(&lsb, "{\"value\":");
                strbf_putn(&lsb, i);
                strbf_puts(&lsb, ",\"title\":\"");
                strbf_puts(&lsb, channels[i]);
                strbf_puts(&lsb, "\"}");
                if(i < j-1) strbf_putc(&lsb, ',');
            }
            strbf_puts(&lsb, "]");
        }
    } else if (!strcmp(name, "log_txt")||!strcmp(name, "logTXT")) {  // switchinf off .txt files
        strbf_putn(&lsb, config->gps.log_txt);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"log to .txt\",\"type\":\"bool\"");
        }
    } else if (!strcmp(name, "log_ubx")||!strcmp(name, "logUBX")) {  // log to .ubx
        strbf_putn(&lsb, config->gps.log_ubx);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"log to .ubx\",\"type\":\"bool\"");
        }
    } else if (!strcmp(name, "log_ubx_nav_sat")||!strcmp(name, "logUBX_nav_sat")) {  // log nav sat msg to .ubx
        strbf_putn(&lsb, config->gps.log_ubx_nav_sat);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"log nav sat msg to .ubx\",\"type\":\"bool\"");
        }
    } else if (!strcmp(name, "log_sbp")||!strcmp(name, "logSBP")) {  // log to .sbp
        strbf_putn(&lsb, config->gps.log_sbp);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"log to .sbp\",\"type\":\"bool\"");
        }
    } else if (!strcmp(name, "log_gpy")||!strcmp(name, "logGPY")) {
        strbf_putn(&lsb, config->gps.log_gpy);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"log to .gpy\",\"type\":\"bool\"");
        }
    }  // log to .gps
    else if (!strcmp(name, "log_gpx")||!strcmp(name, "logGPX")) {
        strbf_putn(&lsb, config->gps.log_gpx);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"log to .gpx\",\"type\":\"bool\"");
        }
    }  // log to .gpx
    else if (!strcmp(name, "file_date_time")) {
        strbf_putn(&lsb, config->file_date_time);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"type of filenaming, with MAC adress or datetime\",\"type\":\"int\",");
            strbf_puts(&lsb, "\"values\":[");
            strbf_puts(&lsb, "{\"value\":1,\"title\":\"name_date_time\"},");
            strbf_puts(&lsb, "{\"value\":0,\"title\":\"name_MAC_index\"},");
            strbf_puts(&lsb, "{\"value\":2,\"title\":\"date_time_name\"}");
            strbf_puts(&lsb, "]");
        }
    }  // type of filenaming, with MAC adress or datetime
    else if (!strcmp(name, "screen_rotation")) {
        strbf_putn(&lsb, config->screen.screen_rotation);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"screen rotation degrees\",\"type\":\"int\",");
            strbf_puts(&lsb, "\"values\":[");
            for(uint8_t i = 0, j = lengthof(screen_rotations); i < j; i++) {
                strbf_puts(&lsb, "{\"value\":");
                strbf_putn(&lsb, i);
                strbf_puts(&lsb, ",\"title\":\"");
                strbf_puts(&lsb, screen_rotations[i]);
                strbf_puts(&lsb, "\"}");
                if(i < j-1) strbf_putc(&lsb, ',');
            }
            strbf_puts(&lsb, "]");
        }
    }  // screen_rotation
    else if (!strcmp(name, "ubx_file")||!strcmp(name, "UBXfile")) {
        strbf_puts(&lsb, "\"");
        strbf_puts(&lsb, config->ubx_file);
        strbf_puts(&lsb, "\"");
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"your preferred filename\",\"type\":\"str\"");
        }
    }  // your preferred filename
    else if (!strcmp(name, "sleep_info")||!strcmp(name, "Sleep_info")) {
        strbf_puts(&lsb, "\"");
        strbf_puts(&lsb, config->sleep_info);
        strbf_puts(&lsb, "\"");
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"your preferred sleep text\",\"type\":\"str\"");
        }
    }  // your preferred sleep text
    else if (strstr(name, "ssid")==name) {
        uint8_t num = 0;
        if (name[4] == 49) num = 1;
        else if(name[4] == 50) num = 2;
        else if(name[4] == 51) num = 3;
        else if(name[4] == 52) num = 4;
        strbf_puts(&lsb, "\"");
        strbf_puts(&lsb, config->wifi_sta[num].ssid);
        strbf_puts(&lsb, "\"");
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"wifi ssid\",\"type\":\"str\"");
        }
    }  // your SSID
    else if (strstr(name, "password")==name) {
        uint8_t num = 0;
        if (name[8] == 49) num = 1;
        else if(name[8] == 50) num = 2;
        else if(name[8] == 51) num = 3;
        else if(name[8] == 52) num = 4;
        strbf_puts(&lsb, "\"");
        strbf_puts(&lsb, config->wifi_sta[num].password);
        strbf_puts(&lsb, "\"");
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"wifi network password\",\"type\":\"str\"");
        }
    }  // your password
    else if (!strcmp(name, "hostname")) {
        strbf_puts(&lsb, "\"");
        strbf_puts(&lsb, config->hostname);
        strbf_puts(&lsb, "\"");
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"hostname: the hostname of the device for present itself in the network\",\"type\":\"str\"");
        }
    }  // your hostname
    if (mode)
        strbf_puts(&lsb, "}");
    *len = lsb.cur - lsb.start;
    DLOG(TAG, "[%s] conf: %s size: %d\n", __FUNCTION__, strbf_finish(&lsb), *len);
    return strbf_finish(&lsb);
}

char *config_get_json(logger_config_t *config, strbf_t *sb, const char *str, uint8_t ublox_hw) {
    ILOG(TAG,"[%s]",__func__);
    IMEAS_START();
    size_t blen = 8 * BUFSIZ, len = 0;
    char buf[blen], *p = 0;
#define CONF_GETC(a)                                    \
    p = config_get(config, a, buf, &len, blen, 1, ublox_hw); \
    if (len) {                                          \
        strbf_puts(sb, p);                               \
    }
#define CONF_GET(a) \
    CONF_GETC(a)   \
    if (len)        \
    strbf_putc(sb, ',')
    if (str) {
        CONF_GETC(str);
    }
    IMEAS_END(TAG, "[%s] took %llu us", __FUNCTION__);
    return strbf_finish(sb);  // str size 6444
#undef CONF_GET
#undef CONF_GETC
}

char *config_encode_json(logger_config_t *config, strbf_t *sb, uint8_t ublox_hw) {
    ILOG(TAG,"[%s]",__func__);
    size_t blen = BUFSIZ / 3 * 2, len = 0;
    char buf[blen], *p = 0;
#define CONF_GETC(a)                                    \
    p = config_get(config, a, buf, &len, blen, 0, ublox_hw); \
    if (len) {                                          \
        strbf_puts(sb, p);                               \
    }
#define CONF_GET(a) \
    CONF_GETC(a)   \
    if (len)        \
    strbf_puts(sb, ",\n")

    strbf_puts(sb, "{\n");
    CONF_GET("hostname");
    CONF_GET("speed_unit");
    CONF_GET("sample_rate");
    CONF_GET("gnss");
    CONF_GET("speed_field");
    CONF_GET("speed_large_font");
    CONF_GET("bar_length");
    CONF_GET("stat_screens");
    CONF_GET("stat_screens_time");
    CONF_GET("stat_speed");
    CONF_GET("archive_days");
    CONF_GET("gpio12_screens");
    CONF_GET("screen_move_offset");
    CONF_GET("board_logo");
    CONF_GET("sail_logo");
    CONF_GET("log_txt");
    CONF_GET("log_ubx");
    CONF_GET("log_ubx_nav_sat");
    CONF_GET("log_sbp");
    CONF_GET("log_gpy");
    CONF_GET("log_gpx");
    CONF_GET("file_date_time");
    CONF_GET("dynamic_model");
    CONF_GET("timezone");
    CONF_GET("ubx_file");
    for(uint8_t i=0,j=L_CONFIG_SSID_MAX; i<j; i++) {
        if(i>0 && config->wifi_sta[i].ssid[0] == 0) break;
        char ssid[12]={0}, password[12]={0};
        memcpy(&ssid[0], "ssid", 4);
        memcpy(&password[0], "password", 8);
        if(i>0) ssid[4] = password[8] = i+48;
        CONF_GET(&ssid[0]);
        CONF_GET(&password[0]);
    }
    CONF_GET("sleep_info");
    CONF_GET("update_enabled");
    CONF_GET("update_channel");
    CONF_GETC("screen_rotation");
    
    strbf_puts(sb, "\n}\n");
    return strbf_finish(sb);
#undef CONF_GETC
#undef CONF_GET
}
