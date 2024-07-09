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
#include "logger_common.h"

static const char *TAG = "config";
SemaphoreHandle_t c_sem_lock = 0;

ESP_EVENT_DEFINE_BASE(CONFIG_EVENT);
TIMER_INIT

//logger_config_t m_config = {0};

#ifdef USE_CUSTOM_CALIBRATION_VAL
cosnt char *config_item_names = "cal_bat,speed_unit,sample_rate,gnss,speed_field,speed_large_font,bar_length,stat_speed,archive_days,sleep_off_screen,file_date_time,dynamic_model,timezone,ssid,password,ublox_type,stat_screens,stat_screens_time,gpio12_screens,board_logo,sail_logo,log_txt,log_ubx,log_ubx_nav_sat,log_sbp,log_gpy,log_gpx,ubx_file,sleep_info";
#else
const char *config_item_names = "speed_unit,sample_rate,gnss,speed_field,speed_large_font,bar_length,stat_speed,archive_days,sleep_off_screen,file_date_time,dynamic_model,timezone,ssid,password,ublox_type,stat_screens,stat_screens_time,gpio12_screens,board_logo,sail_logo,log_txt,log_ubx,log_ubx_nav_sat,log_sbp,log_gpy,log_gpx,ubx_file,sleep_info";
const char *config_item_names_compat = "Stat_screens,Stat_screens_time,GPIO12_screens,Board_Logo,board_Logo,sail_Logo,Sail_Logo,logTXT,logSBP,logUBX,logUBX_nav_sat,logGPY,logGPX,UBXfile,Sleep_info";
#endif

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
    esp_event_post(CONFIG_EVENT, LOGGER_CONFIG_EVENT_CONFIG_INIT_DONE, config, sizeof(logger_config_t), portMAX_DELAY);
    return config;
}

void config_deinit(logger_config_t *config) {
    if(c_sem_lock)
        vSemaphoreDelete(c_sem_lock);
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
    ILOG(TAG,"[%s]",__func__);
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
        printf("[%s] ! var\n", __FUNCTION__);
        goto err;
    }
    if (!strstr(config_item_names, var) && !strstr(config_item_names_compat, var)) {
        printf("[%s] ! in names\n", __FUNCTION__);
        goto err;
    }
    if (!value) {
        printf("[%s] ! value\n", __FUNCTION__);
        goto err;
    }
    #ifdef DEBUG
    if (name || str)
        printf("[%s] {name: ( %s | %s )}\n", __FUNCTION__, (name && name->data.string_ ? name->data.string_ : "-"), (str ? str : "-"));
    if (value)
        printf("[%s] {value: ( %s | %f ), key: %s}\n", __FUNCTION__, (value->tag == JSON_STRING ? value->data.string_ : "-"), (value->tag == JSON_NUMBER ? value->data.number_ : 0), (value->key ? value->key : "-"));
    #endif
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
        if (force || val != config->speed_unit) {
            config->speed_unit = val;
            changed = 1;
        }

    } else if (!strcmp(var, "sample_rate")) {  // gps_rate in Hz, 1, 5 or 10Hz !!!
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        uint8_t val = value->data.number_;
        if (force || val != config->sample_rate) {
            config->sample_rate = val;
            changed = 1;
        }

    } else if (!strcmp(var, "gnss")) {  // default setting 2 GNSS, GPS & GLONAS
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        uint8_t val = value->data.number_;
        if (force || val != config->gnss) {
            config->gnss = val;
            changed = 1;
        }

    } else if (!strcmp(var, "speed_field")) {  // choice for first field in speed screen !!!
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        uint8_t val = value->data.number_;
        if (force || val != config->speed_field) {
            config->speed_field = val;
            changed = 1;
        }

    }

    else if (!strcmp(var, "speed_large_font")) {  // fonts on the first line are bigger, actual speed
        // font is smaller
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        uint8_t val = value->data.number_;
        if (force || val != config->speed_large_font) {
            config->speed_large_font = val;
            changed = 1;
        }

    } else if (!strcmp(var, "dynamic_model")) {  // choice for dynamic model "Sea",if 0 model "portable"
        // is used !!
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        uint8_t val = value->data.number_;
        if (force || val != config->dynamic_model) {
            config->dynamic_model = val;
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
        if (force || val != config->stat_screens) {
            config->stat_screens = val;
            changed = 1;
        }

    } else if (!strcmp(var, "stat_screens_time") || !strcmp(var, "Stat_screens_time")) {  // time between switching stat_screens
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        uint8_t val = value->data.number_;
        if (force || val != config->stat_screens_time) {
            config->stat_screens_time = val;
            changed = 1;
        }

    } else if (!strcmp(var, "gpio12_screens") || !strcmp(var, "GPIO12_screens")) {  // choice for stats field when gpio12 is activated
        // (pull-up high, low = active)
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        uint32_t val = value->data.number_;
        if (force || val != config->gpio12_screens) {
            config->gpio12_screens = val;
            changed = 1;
        }

    } else if (!strcmp(var, "stat_screens_persist")) {  // choice for stats field when no speed, here
        // stat_screen 1, 2 and 3 will be active / for
        // resave the config
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        uint8_t val = value->data.number_;
        if (force || val != config->stat_screens_persist) {
            config->stat_screens_persist = val;
            changed = 1;
        }

    } else if (!strcmp(var, "gpio12_screens_persist")) {  // choice for stats field when gpio12 is
        // activated (pull-up high, low = active) / for
        // resave the config
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        uint8_t val = value->data.number_;
        if (force || val != config->gpio12_screens_persist) {
            config->gpio12_screens_persist = val;
            changed = 1;
        }

    } else if (!strcmp(var, "board_logo") || !strcmp(var, "board_Logo") || !strcmp(var, "Board_Logo")) {
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        uint8_t val = value->data.number_;
        if (force || val != config->board_Logo) {
            config->board_Logo = val;
            changed = 1;
        }

    } else if (!strcmp(var, "sail_logo") || !strcmp(var, "sail_Logo") || !strcmp(var, "Sail_Logo")) {
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        uint8_t val = value->data.number_;
        if (force || val != config->sail_Logo) {
            config->sail_Logo = val;
            changed = 1;
        }

    } else if (!strcmp(var, "sleep_off_screen")) {
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        uint8_t val = value->data.number_;
        if (force || val != config->sleep_off_screen) {
            config->sleep_off_screen = val;
            changed = 1;
        }

    } else if (!strcmp(var, "stat_speed")) {  // max speed in m/s for showing Stat screens
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        uint8_t val = value->data.number_;
        if (force || val != config->stat_speed) {
            config->stat_speed = val;
            changed = 1;
        }

    } else if (!strcmp(var, "bar_length")) {  // choice for bar indicator for length of run in m
        // (nautical mile)
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        uint16_t val = value->data.number_;
        if (force || val != config->bar_length) {
            config->bar_length = val;
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

    } else if (!strcmp(var, "log_txt") || !strcmp(var, "logTXT")) {  // switchinf off .txt files
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        uint8_t val = value->data.number_;
        if (force || val != config->log_txt) {
            config->log_txt = val;
            changed = 1;
        }

    } else if (!strcmp(var, "log_ubx") || !strcmp(var, "logUBX")) {  // log to .ubx
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        uint8_t val = value->data.number_;
        if (force || val != config->log_ubx) {
            config->log_ubx = val;
            changed = 1;
        }

    } else if (!strcmp(var, "log_ubx_nav_sat") || !strcmp(var, "logUBX_nav_sat")) {  // log nav sat msg to .ubx
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        uint8_t val = value->data.number_;
        if (force || val != config->log_ubx_nav_sat) {
            config->log_ubx_nav_sat = val;
            changed = 1;
        }

    } else if (!strcmp(var, "log_sbp") || !strcmp(var, "logSBP")) {  // log to .sbp
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        uint8_t val = value->data.number_;
        if (force || val != config->log_sbp) {
            config->log_sbp = val;
            changed = 1;
        }

    } else if (!strcmp(var, "log_gpy") || !strcmp(var, "logGPY")) {
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        uint8_t val = value->data.number_;
        if (force || val != config->log_gpy) {
            config->log_gpy = val;
            changed = 1;
        }

    }  // log to .gps
    else if (!strcmp(var, "log_gpx") || !strcmp(var, "logGPX")) {
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        uint8_t val = value->data.number_;
        if (force || val != config->log_gpx) {
            config->log_gpx = val;
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
    else if (!strcmp(var, "ssid")) {
        if (!value || value->tag != JSON_STRING) {
            goto err;
        }
        const char *val = value->data.string_;
        if (force || strcmp(val, config->ssid)) {
            size_t len = strlen(val);
            memcpy(config->ssid, val, len);
            config->ssid[len] = 0;
            changed = 1;
        }
    }  // your SSID
    else if (!strcmp(var, "password")) {
        if (!value || value->tag != JSON_STRING) {
            goto err;
        }
        const char *val = value->data.string_;
        if (force || strcmp(val, config->password)) {
            size_t len = strlen(val);
            memcpy(config->password, val, len);
            config->password[len] = 0;
            changed = 1;
        }
    }  // your password
    else if (!strcmp(var, "ublox_type")) {
        if (!value || value->tag != JSON_NUMBER) {
            goto err;
        }
        int8_t val = value->data.number_;
        if(val < 0) val = 0;
        if (force || val != config->ublox_type) {
            config->ublox_type = val;
            changed = 1;
        }
    } else {
    err:
        ESP_LOGW(TAG, "[%s] error: %s %d", __FUNCTION__, var ? var : "-", value ? value->tag : -1);
        /* if(root) {
            char * x = json_stringify(root, "\n");
            printf("%s", x);
            free(x);
        } */
        changed = -2;
    }
    return changed;
}

int config_set_var(logger_config_t *config, const char *json, const char *var) {
    #ifdef DEBUG
    ESP_LOGI(TAG, "[%s] '%s'", __FUNCTION__, json);
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

int config_save_var(struct logger_config_s *config, const char * filename, const char * filename_b, const char *json, const char *var, uint8_t ublox_hw) {
    TIMER_S
    xSemaphoreTake(c_sem_lock, portMAX_DELAY);
    int ret = config_set_var(config, json, var);
    if (ret > 0) {
        ret = config_save_json(config, filename, filename_b, ublox_hw);
    }
    xSemaphoreGive(c_sem_lock);
    TIMER_E
    return ret;
}

int config_save_var_b(logger_config_t *config, const char * filename, const char * filename_b, const char *json, uint8_t ublox_hw) {
    ILOG(TAG,"[%s]",__func__);
    return config_save_var(config, filename, filename_b, json, 0, ublox_hw);
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
    changed = SET_CONF(root, "sleep_off_screen");
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
    changed = SET_CONF(root, "timezone");
    changed = SET_CONF(root, "ubx_file");
    if (changed <= -2)
        changed = SET_CONF(root, "UBXfile");
    changed = SET_CONF(root, "sleep_info");
    if (changed <= -2)
        changed = SET_CONF(root, "Sleep_info");
    changed = SET_CONF(root, "ssid");
    changed = SET_CONF(root, "password");
    changed = SET_CONF(root, "ublox_type");

    if (root)
        json_delete(root);

    if (ret == ESP_FAIL) {
        //ESP_LOGW(TAG, "cal_bat:     %.2f", config->cal_bat);
        ESP_LOGW(TAG, "speed_unit:   %hhu", config->speed_unit);
        ESP_LOGW(TAG, "sample_rate: %d", config->sample_rate);
        ESP_LOGW(TAG, "log_sbp:     %c", config->log_sbp);
        ESP_LOGW(TAG, "log_ubx:     %c", config->log_ubx);
        ESP_LOGW(TAG, "ssid:        %s", config->ssid);
        ESP_LOGW(TAG, "password:    %s", config->password);
        ESP_LOGW(TAG, "sail_logo:   %d", config->sail_Logo);
    }
    return ret;
#undef SET_CONF
}

esp_err_t config_load_json(logger_config_t *config, const char *filename, const char *filename_backup) {
    TIMER_S
    int ret = ESP_OK;
    char *json = 0;
    ESP_LOGI(TAG, "configuration will read from %s", filename);
    xSemaphoreTake(c_sem_lock, portMAX_DELAY);
    if ((json = s_read_from_file(filename, CONFIG_SD_MOUNT_POINT))) {
        ESP_LOGI(TAG, "configuration read from %s", filename);
    } else if ((json = s_read_from_file(filename_backup, CONFIG_SD_MOUNT_POINT))) {
        ESP_LOGI(TAG, "configuration read from %s", filename_backup);
    } else {
        ESP_LOGE(TAG, "Configuration not found...");
        goto done;
    }
    //printf("conf:%s\n", json);
    ret = config_decode(config, json);
done:
    xSemaphoreGive(c_sem_lock);
    if (json)
        free(json);
    esp_event_post(CONFIG_EVENT, LOGGER_CONFIG_EVENT_CONFIG_LOAD_DONE, config, sizeof(logger_config_t), portMAX_DELAY);
    TIMER_E
    return ret;
}

esp_err_t config_save_json(logger_config_t *config, const char *filename, const char *filename_backup, uint8_t ublox_hw) {
    int ret = ESP_OK;
    strbf_t sb;
    strbf_init(&sb);
    char *json = config_encode_json(config, &sb, ublox_hw);
    if (!json_validate(json)) {
        ESP_LOGE(TAG, "Bad json: %s", json);
        goto done;
    #ifdef DEBUG
    } else {
        printf("Going to save json %s", sb.start);
    #endif
    }
    s_rename_file(filename, filename_backup, CONFIG_SD_MOUNT_POINT);
    ret = s_write(filename, CONFIG_SD_MOUNT_POINT, sb.start, sb.cur - sb.start);
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
        config->log_txt = 1;  // because txt file is needed for generating new file count !!
    if (config->stat_screens_time < 1)
        config->stat_screens_time = 1;
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
        if (orig->speed_unit != config->speed_unit)
            return 2;
        if (orig->sample_rate != config->sample_rate)
            return 3;
        if (orig->gnss != config->gnss)
            return 4;
        if (orig->speed_field != config->speed_field)
            return 5;
        if (orig->speed_large_font != config->speed_large_font)
            return 6;
        if (orig->bar_length != config->bar_length)
            return 7;
        if (orig->stat_speed != config->stat_speed)
            return 8;
        if (orig->archive_days != config->archive_days)
            return 9;
        if (orig->stat_screens_time != config->stat_screens_time)
            return 10;
        if (orig->stat_screens != config->stat_screens)
            return 11;
        if (orig->stat_screens_persist != config->stat_screens_persist)
            return 12;
        if (orig->gpio12_screens != config->gpio12_screens)
            return 13;
        if (orig->gpio12_screens_persist != config->gpio12_screens_persist)
            return 14;
        if (orig->board_Logo != config->board_Logo)
            return 15;
        if (orig->sail_Logo != config->sail_Logo)
            return 16;
        if (orig->sleep_off_screen != config->sleep_off_screen)
            return 17;
        if (orig->log_txt != config->log_txt)
            return 18;
        if (orig->log_ubx != config->log_ubx)
            return 19;
        if (orig->log_ubx_nav_sat != config->log_ubx_nav_sat)
            return 2;
        if (orig->log_sbp != config->log_sbp)
            return 21;
        if (orig->log_gpy != config->log_gpy)
            return 22;
        if (orig->file_date_time != config->file_date_time)
            return 23;
        if (orig->dynamic_model != config->dynamic_model)
            return 24;
        if (orig->timezone != config->timezone)
            return 25;
        if (strcmp(config->ubx_file, orig->ubx_file))
            return 26;
        if (strcmp(config->sleep_info, orig->sleep_info))
            return 27;
        if (strcmp(config->ssid, orig->ssid))
            return 28;
        if (strcmp(config->password, orig->password))
            return 29;
        if (orig->speed_field_count != config->speed_field_count)
            return 30;
    }
    return 0;
}

char *config_get(logger_config_t *config, const char *name, char *str, size_t *len, size_t max, uint8_t mode, uint8_t ublox_hw) {
    ESP_LOGI(TAG, "[%s] %s", __FUNCTION__, name);
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
        strbf_putn(&lsb, config->speed_unit);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"m/s, km/h or knots\",\"type\":\"int\",\"values\":[");
            strbf_puts(&lsb, "{\"value\":0,\"title\":\"m/s\"},");
            strbf_puts(&lsb, "{\"value\":1,\"title\":\"km/h\"},");
            strbf_puts(&lsb, "{\"value\":2,\"title\":\"knots\"}");
            strbf_puts(&lsb, "]");
        }
    } else if (!strcmp(name, "sample_rate")) {  // gps_rate in Hz, 1, 5 or 10Hz !!!
        strbf_putn(&lsb, config->sample_rate);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"gps_rate in Hz\",\"type\":\"int\",\"values\":[{\"value\":1,\"title\":\"1Hz\"},{\"value\":5,\"title\":\"5Hz\"},{\"value\":10,\"title\":\"10Hz\"}");
            strbf_puts(&lsb, ublox_hw >= UBX_TYPE_M9 ? ",{\"value\":20,\"title\":\"20Hz\"}]" : "]");
            strbf_puts(&lsb, ",\"ext\":\"Hz\"");
        }
    } else if (!strcmp(name, "gnss")) {  // default setting 2 GPS + GLONAS
        strbf_putn(&lsb, config->gnss);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"gnss choice : GPS + GLONAS + GALILEO (M8 ublox ROM version 3.01, M10).<br> Some Beitian modules still have a old firmware, ROM 2.01. Here, Galileo can't be activated. M9 can do 4 GNSS simultan!\",\"type\":\"int\",");
            strbf_puts(&lsb, "\"values\":[{\"value\":2,\"title\":\"GPS + GLONAS\"},");
            strbf_puts(&lsb, "{\"value\":3,\"title\":\"GPS + GALILEO + GLONASS\"},");
            strbf_puts(&lsb, "{\"value\":4,\"title\":\"GPS + GALILEO + BEIDOU\"}");
            if (ublox_hw >= UBX_TYPE_M9) {
                strbf_puts(&lsb, ",{\"value\":5,\"title\":\"GPS + GALILEO + BEIDOU + GLONAS\"}");
            }
            strbf_puts(&lsb, "]");
        }
    } else if (!strcmp(name, "speed_field")) {  // choice for first field in speed screen !!!
        strbf_putn(&lsb, config->speed_field);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"choice for first field in speed screen !!!\",\"type\":\"int\"");
        }
    }

    else if (!strcmp(name, "speed_large_font")) {  // fonts on the first line are bigger, actual speed font is smaller
        strbf_putn(&lsb, config->speed_large_font);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"fonts on the first line are bigger, actual speed font is smaller\",\"type\":\"int\",\"values\":[");
            strbf_puts(&lsb, "{\"value\":0,\"title\":\"Large_Font OFF\"},");
            strbf_puts(&lsb, "{\"value\":1,\"title\":\"Large_Font ON\"}");
            strbf_puts(&lsb, "]");
        }
    } else if (!strcmp(name, "dynamic_model")) {  // choice for dynamic model "Sea",if 0 model "portable" is used !!
        strbf_putn(&lsb, config->dynamic_model);
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
            strbf_puts(&lsb, ",\"info\":\"timezone: The local time difference in hours with UTC (can be fractional/negative!) <a href='https://en.wikipedia.org/wiki/List_of_UTC_offsets' target='_blank'>Wikipedia</a>\",\"type\":\"float\",\"ext\":\"h\",");
            strbf_puts(&lsb, "\"values\":[");
            strbf_puts(&lsb, "{\"value\":-12,\"title\":\"GMT-12\"},");
            strbf_puts(&lsb, "{\"value\":-11,\"title\":\"GMT-11\"},");
            strbf_puts(&lsb, "{\"value\":-10,\"title\":\"GMT-10\"},");
            strbf_puts(&lsb, "{\"value\":-9,\"title\":\"GMT-9\"},");
            strbf_puts(&lsb, "{\"value\":-8,\"title\":\"GMT-8\"},");
            strbf_puts(&lsb, "{\"value\":-7,\"title\":\"GMT-7\"},");
            strbf_puts(&lsb, "{\"value\":-6,\"title\":\"GMT-6\"},");
            strbf_puts(&lsb, "{\"value\":-5,\"title\":\"GMT-5\"},");
            strbf_puts(&lsb, "{\"value\":-4,\"title\":\"GMT-4\"},");
            strbf_puts(&lsb, "{\"value\":-3,\"title\":\"GMT-3\"},");
            strbf_puts(&lsb, "{\"value\":-2,\"title\":\"GMT-2\"},");
            strbf_puts(&lsb, "{\"value\":-1,\"title\":\"GMT-1\"},");
            strbf_puts(&lsb, "{\"value\":0,\"title\":\"GMT\"},");
            strbf_puts(&lsb, "{\"value\":1,\"title\":\"GMT+1\"},");
            strbf_puts(&lsb, "{\"value\":2,\"title\":\"GMT+2\"},");
            strbf_puts(&lsb, "{\"value\":3,\"title\":\"GMT+3\"},");
            strbf_puts(&lsb, "{\"value\":4,\"title\":\"GMT+4\"},");
            strbf_puts(&lsb, "{\"value\":5,\"title\":\"GMT+5\"},");
            strbf_puts(&lsb, "{\"value\":6,\"title\":\"GMT+6\"},");
            strbf_puts(&lsb, "{\"value\":7,\"title\":\"GMT+7\"},");
            strbf_puts(&lsb, "{\"value\":8,\"title\":\"GMT+8\"},");
            strbf_puts(&lsb, "{\"value\":9,\"title\":\"GMT+9\"},");
            strbf_puts(&lsb, "{\"value\":10,\"title\":\"GMT+10\"},");
            strbf_puts(&lsb, "{\"value\":11,\"title\":\"GMT+11\"},");
            strbf_puts(&lsb, "{\"value\":12,\"title\":\"GMT+12\"},");
            strbf_puts(&lsb, "{\"value\":13,\"title\":\"GMT+13\"}");
            strbf_puts(&lsb, "]");
        }                                        // 2575
    } else if (!strcmp(name, "stat_screens")||!strcmp(name, "Stat_screens")) {  // choice for stats field when no speed, here stat_screen
        // 1, 2 and 3 will be active
        strbf_putn(&lsb, config->stat_screens);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"Stat_screens choice : every digit shows the according stat_screen after each other\",\"type\":\"int\"");
        }
    } else if (!strcmp(name, "stat_screens_time")||!strcmp(name, "Stat_screens_time")) {  // time between switching stat_screens
        strbf_putn(&lsb, config->stat_screens_time);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"The time between toggle the different stat screens\",\"type\":\"int\"");
        }
    } else if (!strcmp(name, "gpio12_screens")||!strcmp(name, "GPIO12_screens")) {  // choice for stats field when gpio12 is activated
        // (pull-up high, low = active)
        strbf_putn(&lsb, config->gpio12_screens);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"GPIO12_screens choice : Every digit shows the according GPIO_screen after each push. Screen 4 = s10 runs, screen 5 = alfa's.\",\"type\":\"int\"");
        }
    } else if (!strcmp(name, "stat_screens_persist")) {  // choice for stats field when no speed, here
        // stat_screen 1, 2 and 3 will be active / for
        // resave the config
        strbf_putn(&lsb, config->stat_screens_persist);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"The time between toggle the different stat screens\",\"type\":\"int\"");
        }
    } else if (!strcmp(name, "gpio12_screens_persist")) {  // choice for stats field when gpio12 is
        // activated (pull-up high, low = active) / for
        // resave the config
        strbf_putn(&lsb, config->gpio12_screens_persist);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"choice for stats field when gpio12 is activated (pull-up high, low = active) / for resave the config\",\"type\":\"int\"");
        }
    } else if (!strcmp(name, "board_logo")||!strcmp(name, "board_logo")||!strcmp(name, "Board_Logo")) {
        strbf_putn(&lsb, config->board_Logo);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"Board_Logo: from 1 - 20. See the info on <a href='https://www.seabreeze.com.au/img/photos/windsurfing/19565287.jpg' target='_blank'>Seabreeze</a>, bigger than are 10 different single logos\",\"type\":\"int\"");
        }
    } else if (!strcmp(name, "sail_logo")||!strcmp(name, "sail_logo")||!strcmp(name, "Sail_Logo")) {
        strbf_putn(&lsb, config->sail_Logo);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"Sail_Logo: from 1 - 20. See the info on <a href='https://www.seabreeze.com.au/img/photos/windsurfing/19565287.jpg' target='_blank'>Seabreeze</a>, bigger than are 10 different single logos\",\"type\":\"int\"");
        }
    } else if (!strcmp(name, "sleep_off_screen")) {
        strbf_putn(&lsb, config->sleep_off_screen);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"Choice for switch_off (first digit 0 or 1) and sleep_screen (second digit 0 or 1):\",\"type\":\"int\"");
        }
    } else if (!strcmp(name, "stat_speed")) {  // max speed in m/s for showing Stat screens
        strbf_putn(&lsb, config->stat_speed);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"max speed in m/s for showing Stat screens\",\"type\":\"int\",\"ext\":\"m/s\"");
        }
    } else if (!strcmp(name, "bar_length")) {  // choice for bar indicator for length of run in m
        // (nautical mile)
        strbf_putn(&lsb, config->bar_length);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"bar_length: Default length = 1852 m for 100% bar (=Nautical mile)\",\"type\":\"int\",\"ext\":\"m\"");
        }
    } else if (!strcmp(name, "archive_days")) {  // how many days files will be moved to the "Archive" dir
        strbf_putn(&lsb, config->archive_days);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"how many days files will be moved to the 'Archive' dir\",\"type\":\"int\",\"ext\":\"d\"");
        }
    } else if (!strcmp(name, "log_txt")||!strcmp(name, "logTXT")) {  // switchinf off .txt files
        strbf_putn(&lsb, config->log_txt);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"log to .txt\",\"type\":\"bool\"");
        }
    } else if (!strcmp(name, "log_ubx")||!strcmp(name, "logUBX")) {  // log to .ubx
        strbf_putn(&lsb, config->log_ubx);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"log to .ubx\",\"type\":\"bool\"");
        }
    } else if (!strcmp(name, "log_ubx_nav_sat")||!strcmp(name, "logUBX_nav_sat")) {  // log nav sat msg to .ubx
        strbf_putn(&lsb, config->log_ubx_nav_sat);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"log nav sat msg to .ubx\",\"type\":\"bool\"");
        }
    } else if (!strcmp(name, "log_sbp")||!strcmp(name, "logSBP")) {  // log to .sbp
        strbf_putn(&lsb, config->log_sbp);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"log to .sbp\",\"type\":\"bool\"");
        }
    } else if (!strcmp(name, "log_gpy")||!strcmp(name, "logGPY")) {
        strbf_putn(&lsb, config->log_gpy);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"log to .gpy\",\"type\":\"bool\"");
        }
    }  // log to .gps
    else if (!strcmp(name, "log_gpx")||!strcmp(name, "logGPX")) {
        strbf_putn(&lsb, config->log_gpx);
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
    else if (!strcmp(name, "ssid")) {
        strbf_puts(&lsb, "\"");
        strbf_puts(&lsb, config->ssid);
        strbf_puts(&lsb, "\"");
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"ssid: the name of the wlan where the esp-logger should connect to\",\"type\":\"str\"");
        }
    }  // your SSID
    else if (!strcmp(name, "password")) {
        strbf_puts(&lsb, "\"");
        strbf_puts(&lsb, config->password);
        strbf_puts(&lsb, "\"");
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"password: the password of the wlan where the esp-logger should connect to\",\"type\":\"str\"");
        }
    }  // your password
    else if (!strcmp(name, "ublox_type")) {
        strbf_putn(&lsb, config->ublox_type);
        if (mode) {
            strbf_puts(&lsb, ",\"info\":\"ublox_type\",\"type\":\"int\",");
            strbf_puts(&lsb, "\"values\":[");
            strbf_puts(&lsb, "{\"value\":1,\"title\":\"M8 9600Bd\"},");
            strbf_puts(&lsb, "{\"value\":3,\"title\":\"M8 38400Bd\"},");
            strbf_puts(&lsb, "{\"value\":5,\"title\":\"M9 9600Bd\"},");
            strbf_puts(&lsb, "{\"value\":6,\"title\":\"M9 38400Bd\"},");
            strbf_puts(&lsb, "{\"value\":2,\"title\":\"M10 9600Bd\"},");
            strbf_puts(&lsb, "{\"value\":4,\"title\":\"M10 38400Bd\"},");
            strbf_puts(&lsb, "{\"value\":0,\"title\":\"Autoselect\"}");
            strbf_puts(&lsb, "]");
        }
    }
    if (mode)
        strbf_puts(&lsb, "}");
    *len = lsb.cur - lsb.start;
    #ifdef DEBUG
    printf("conf: %s size: %d\n", strbf_finish(&lsb), *len);
    #endif
    return strbf_finish(&lsb);
}

char *config_get_json(logger_config_t *config, strbf_t *sb, const char *str, uint8_t ublox_hw) {
    TIMER_S
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
    // } else {
    //     strbf_puts(sb, "[");
    //     //CONF_GET("cal_bat");
    //     CONF_GET("speed_unit");
    //     CONF_GET("sample_rate");
    //     CONF_GET("gnss");
    //     CONF_GET("speed_field");
    //     CONF_GET("speed_large_font");
    //     CONF_GET("bar_length");
    //     CONF_GET("stat_screens");
    //     CONF_GET("stat_screens_time");
    //     CONF_GET("stat_speed");
    //     CONF_GET("archive_days");
    //     CONF_GET("gpio12_screens");
    //     CONF_GET("board_Logo");
    //     CONF_GET("sail_Logo");
    //     CONF_GET("sleep_off_screen");
    //     CONF_GET("log_txt");
    //     CONF_GET("log_ubx");
    //     CONF_GET("log_ubx_nav_sat");
    //     CONF_GET("log_sbp");
    //     CONF_GET("log_gpy");
    //     CONF_GET("log_gpx");
    //     CONF_GET("file_date_time");
    //     CONF_GET("dynamic_model");
    //     CONF_GET("timezone");
    //     CONF_GET("ubx_file");
    //     CONF_GET("sleep_info");
    //     CONF_GET("ssid");
    //     CONF_GET("password");
    //     CONF_GETC("ublox_type");
    //     strbf_puts(sb, "]\n");
    }
    TIMER_E
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
    //CONF_GET("cal_bat");
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
    CONF_GET("board_logo");
    CONF_GET("sail_logo");
    CONF_GET("sleep_off_screen");
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
    CONF_GET("sleep_info");
    CONF_GET("ssid");
    CONF_GET("password");
    CONF_GETC("ublox_type");

    strbf_puts(sb, "\n}\n");
    return strbf_finish(sb);
#undef CONF_GETC
#undef CONF_GET
}
