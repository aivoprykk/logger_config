#ifndef CC351F44_DBE2_4CB7_9FBA_52E0AC84322E
#define CC351F44_DBE2_4CB7_9FBA_52E0AC84322E

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "logger_common.h"

// extern const char * const config_speed_field_items[];
// extern const char * const config_screen_items[];
// extern const char * const config_gps_items[];
// extern const char * const config_fw_update_items[];
extern const size_t config_speed_field_item_count;
extern const size_t config_screen_item_count;
// extern const size_t config_gps_item_count;
extern const size_t config_fw_update_item_count;

// configuration item names in char array
extern const char * const config_items[];
extern const size_t config_item_count;

// configuration item names in | separated string
extern const char *config_item_names;

#if defined(CUSTOM_CALIBRATION_VAL)
#define CFG_CALIBRATION_ITEM_LIST(l) l(cal_bat)
#else
#define CFG_CALIBRATION_ITEM_LIST(l)
#endif

// #define CFG_GPS_ITEM_LIST(l) l(gnss) l(sample_rate) l(timezone) l(speed_unit) l(log_txt) l(log_ubx) l(log_sbp) l(log_gpy) l(log_gpx) l(log_ubx_nav_sat) l(dynamic_model)
#define CFG_SCREEN_ITEM_LIST(l) l(speed_field) l(stat_screens_time) l(board_logo) l(sail_logo) l(screen_rotation)
#define CGG_SCREEN_ITEM_ROTATION_POS (4)
#if defined(CONFIG_LCD_IS_EPD)
#define CFG_SCREEN_ITEM_LIST_A(l) l(screen_move_offset)
#else
#define CFG_SCREEN_ITEM_LIST_A(l) l(screen_brightness)
#define CGG_SCREEN_ITEM_BRIGHTNESS_POS (CGG_SCREEN_ITEM_ROTATION_POS+1)
#endif
#define CFG_FW_UPDATE_ITEM_LIST(l) l(update_enabled) l(update_channel)
#define CFG_ITEM_LIST(l) l(speed_large_font) l(bar_length) l(stat_speed) l(archive_days) l(ssid) l(password) l(ssid1) l(password1) l(ssid2) l(password2) l(ssid3) l(password3) l(gpio12_screens) l(sleep_info) l(hostname)
#define SPEED_FIELD_ITEM_LIST(l) l(dynamic) l(spd_10_sec) l(spd_alpha) l(spd_1852_m) l(spd_500_m) l(spd_dist_time) l(spd_max_2s_10s) l(spd_half_hour) l(spd_1_hour) l(spd_1h_dynamic)

#define CFG_ENUM(l) cfg_##l,

// configuration items in enum
typedef enum {
    CFG_CALIBRATION_ITEM_LIST(CFG_ENUM)
    CFG_SCREEN_ITEM_LIST(CFG_ENUM)
    CFG_SCREEN_ITEM_LIST_A(CFG_ENUM)
    CFG_FW_UPDATE_ITEM_LIST(CFG_ENUM)
    CFG_ITEM_LIST(CFG_ENUM)
} config_item_t;

// typedef struct logger_config_gps_s {
//     uint8_t gnss;             // default setting 2 GNSS, GPS & GLONAS
//     uint8_t sample_rate;      // gps_rate in Hz, 1, 5 or 10Hz !!!
//     uint8_t speed_unit;       // 0 = m/s, 1 = km/h, 2 = knots
//     uint8_t log_txt;          // switchinf off .txt files
//     uint8_t log_ubx;          // log to .ubx
//     uint8_t log_sbp;          // log to .sbp
//     uint8_t log_gpy;          // log to .gps
//     uint8_t log_gpx;          // log to .gpx
//     uint8_t log_ubx_nav_sat;  // log nav sat msg to .ubx
//     uint8_t dynamic_model;    // choice for dynamic model "Sea",if 0 model "portable" is used !!
// } logger_config_gps_t;
// // #define L_CONFIG_GPS_FIELDS sizeof(struct logger_config_gps_s)
/* #define LOGGER_CONFIG_GPS_DEFAULTS() { \
//     .gnss = 111, \
//     .sample_rate = 10, \
//     .speed_unit = 1, \
//     .log_txt = true, \
//     .log_ubx = true, \
//     .log_sbp = false, \
//     .log_gpy = false, \
//     .log_gpx = false, \
//     .log_ubx_nav_sat = false, \
//     .dynamic_model = 0, \
// }*/

typedef struct logger_config_speed_field_s {
    uint8_t dynamic;
    uint8_t stat_10_sec;
    uint8_t stat_alpha;
    uint8_t stat_1852_m;
    uint8_t stat_500_m;
    uint8_t stat_dist_time;
    uint8_t stat_max_2s_10s;
    uint8_t stat_half_hour;
    uint8_t stat_1_hour;
    uint8_t stat_1_hour_dynamic;
} logger_config_speed_field_t;
#define L_CONFIG_SPEED_FIELDS sizeof(struct logger_config_speed_field_s)
#define LOGGER_CONFIG_SPEED_FIELS_DEFAULTS() { \
    .dynamic = 1, \
    .stat_10_sec = 0, \
    .stat_alpha = 0, \
    .stat_1852_m = 0, \
    .stat_dist_500m = 0, \
    .stat_max_2s_10s = 0, \
    .stat_half_hour = 0, \
    .stat_1_hour = 0, \
    .stat_1_hour_dynamic = 0, \
}

#if !defined(SCR_DEFAULT_ROTATION)
#if !defined(CONFIG_LCD_IS_EPD)
#define SCR_DEFAULT_ROTATION 2 // 90deg
#else
#define SCR_DEFAULT_ROTATION 1 // 90deg
#endif
#endif
#if !defined(SCR_AUTO_REFRESH)
#if !defined(CONFIG_LCD_IS_EPD)
#define SCR_AUTO_REFRESH (1)
#else
#define SCR_AUTO_REFRESH (0)
#endif
#endif

typedef struct logger_config_screen_s {
    uint8_t speed_field;             // choice for first field in speed screen !!!
    uint8_t speed_large_font;        // fonts on the first line are bigger, actual speed font is smaller
    uint8_t stat_screens_time;       // time between switching stat_screens
    uint8_t board_logo;
    uint8_t sail_logo;
    int8_t screen_rotation;
    uint8_t screen_no_auto_refresh;
    uint8_t stat_speed;       // max speed in m/s for showing Stat screens
    uint16_t gpio12_screens;  // choice for stats field when gpio12 is activated (pull-up high, low = active)
} logger_config_screen_t;
// #define L_CONFIG_SCREEN_FIELDS sizeof(struct logger_config_screen_s)
#define LOGGER_CONFIG_SCREEN_DEFAULTS() { \
    .speed_field = 1, \
    .speed_large_font = 0, \
    .stat_screens_time = 3, \
    .board_logo = 1, \
    .sail_logo = 1, \
    .stat_speed = 1, \
    .screen_rotation = SCR_DEFAULT_ROTATION, \
    .screen_no_auto_refresh = !SCR_AUTO_REFRESH, \
    .gpio12_screens = 255U, \
}

typedef enum {
    FW_UPDATE_CHANNEL_PROD = 0,
    FW_UPDATE_CHANNEL_DEV = 1,
} fw_update_channel_t;

typedef struct logger_config_fwupdate_c {
    bool update_enabled;
    fw_update_channel_t channel;
} logger_config_fwupdate_t;

#if defined(CONFIG_LOGGER_BUILD_MODE_DEV)
#define CFG_CHANNEL FW_UPDATE_CHANNEL_DEV
#else
#define CFG_CHANNEL FW_UPDATE_CHANNEL_PROD
#endif

#define LOGGER_CONFIG_FWUPDATE_DEFAULTS() { \
    .update_enabled = true, \
    .channel = CFG_CHANNEL, \
}

#define L_CONFIG_SSID_MAX 4

typedef struct logger_config_wifi_sta_s {
    char ssid[32];        // your SSID
    char password[32];    // your password
} logger_config_wifi_sta_t;

typedef struct logger_config_s {
    // logger_config_gps_t gps;
    logger_config_screen_t screen;
    // float timezone;           // choice for timedifference in hours with UTC, for Belgium 1 or 2 (summertime)
    // uint8_t file_date_time;   // type of filenaming, with MAC adress or datetime
    uint8_t config_fail;
    int8_t  screen_move_offset;
    uint8_t speed_field_count;
    uint16_t bar_length;      // choice for bar indicator for length of run in m (nautical mile)
    uint8_t screen_brightness;

    uint16_t archive_days;    // how many days files will be moved to the "Archive" dir
 
    char sleep_info[32];  // your preferred sleep text

    struct logger_config_fwupdate_c fwupdate;

    struct logger_config_wifi_sta_s wifi_sta[L_CONFIG_SSID_MAX]; // your SSID and password
    char hostname[32];    // your hostname
    void(*config_changed_screen_cb)(const char *name);
} logger_config_t;

#define LOGGER_CONFIG_DEFAULTS() { \
    .screen = LOGGER_CONFIG_SCREEN_DEFAULTS(), \
    .config_fail = 0, \
    .screen_move_offset = 1, \
    .speed_field_count = L_CONFIG_SPEED_FIELDS, \
    .bar_length = 1852, \
    .screen_brightness = 100, \
    .archive_days = 30, \
    .sleep_info = "ESP GPS", \
    .fwupdate = LOGGER_CONFIG_FWUPDATE_DEFAULTS(), \
    .hostname = "esp", \
    .wifi_sta = { \
        { .ssid = "ssid1", .password = "password1" }, \
        { {0}, {0} }, \
        { {0}, {0} }, \
        { {0}, {0} }, \
    }, \
    .config_changed_screen_cb = NULL, \
}

struct strbf_s;

/*
* @brief Create a new configuration
*/
logger_config_t *config_new(void);

/*
* @brief Free a configuration
* @param config The configuration to free
*/
void config_delete(logger_config_t *config);

/*
* @brief Initialize a configuration
* @param config The configuration to initialize
*/
struct logger_config_s *config_init(struct logger_config_s *config);

/*
* @brief Deinitialize a configuration
* @param config The configuration to deinitialize
*/
void config_deinit(struct logger_config_s *config);

/*
* @brief Load config defaults
* @param config The configuration to load defaults into
*/
struct logger_config_s *config_defaults(struct logger_config_s *config);

/*
* @brief Get a variable from the configuration
* @param config The configuration to get the variable from
* @param name The name of the variable to get
* @param str The string to get the variable into
* @param len The length of the string
* @param max The maximum length of the string
* @param mode The mode to get the variable in
*/
struct strbf_s;
char *config_get(const struct logger_config_s *config, const char *name, struct strbf_s *sb, uint8_t mode);
uint8_t cnf_get_item(const logger_config_t *config, uint8_t pos, struct strbf_s * lsb, uint8_t mode);

/*
* @brief Set a variable in the configuration
* @param config The configuration to set the variable in
* @param name The name of the variable to set
* @param str The string to set the variable from
* @param force The force to set the variable with
*/
int config_set(logger_config_t *config, const char *str, void *root, uint8_t force);

/*
* @brief Load the configuration from a file
* @param config The configuration to load
* @param filename The filename to load the configuration from
* @param filename_backup The filename to load the configuration from
*/
int config_load_json(struct logger_config_s *config);

/*
* @brief Save the configuration to a file
* @param config The configuration to save
* @param filename The filename to save the configuration to
* @param filename_backup The filename to save the configuration to
*/
int config_save_json(struct logger_config_s *config);

/*
* @brief Decode a JSON string into a configuration
* @param config The configuration to save
* @param json The JSON string to decode
*/
int config_decode(struct logger_config_s *config, const char *json);

/*
* @brief Fix values in the configuration
* @param config The configuration to fix values in
*/
struct logger_config_s *config_fix_values(struct logger_config_s *config);

/*
* @brief Compare two configurations
* @param orig The original configuration
* @param config The configuration to compare
*/
int config_compare(struct logger_config_s *orig, struct logger_config_s *config);

/*
* @brief Clone a configuration
* @param orig The original configuration
* @param config The configuration to clone into
*/
struct logger_config_s *config_clone(struct logger_config_s *orig, struct logger_config_s *config);

/*
* @brief Encode a configuration into a JSON string
* @param config The configuration to encode
* @param sb The string builder to use
*/
char *config_encode_json(struct logger_config_s * config, struct strbf_s *sb);

/*
* @brief Set a variable in the configuration
* @param config The configuration to set the variable in
* @param json The JSON string to set the variable from
* @param var The variable to set
*/
int config_set_var(struct logger_config_s *config, const char *json, const char *var);

/*
* @brief Save a variable in the configuration to a file
* @param config The configuration to save the variable from
* @param filename The filename to save the variable to
* @param filename_b The filename to save the variable to
* @param json The JSON string to save the variable from
* @param var The variable to save
*/
int config_save_var(struct logger_config_s *config, const char *json, const char *var);

/*
* @brief Save a variable in the configuration to a file
* @param config The configuration to save the variable from
* @param filename The filename to save the variable to
* @param filename_b The filename to save the variable to
* @param json The JSON string to save the variable from
* @param var The variable to save
*/
int config_save_var_b(struct logger_config_s *config, const char *json);

esp_err_t config_set_screen_cb(logger_config_t * config, void(*cb)(const char *));

// struct m_config_item_s * get_gps_cfg_item(const logger_config_t *config, int num, struct m_config_item_s *item);
// int set_gps_cfg_item(logger_config_t *config, int num);
struct m_config_item_s * get_screen_cfg_item(const logger_config_t *config, int num, struct m_config_item_s *item);
int set_screen_cfg_item(logger_config_t * config, int num);
struct m_config_item_s * get_fw_update_cfg_item(const logger_config_t *config, int num, struct m_config_item_s *item);
int set_fw_update_cfg_item(logger_config_t * config, int num);

#ifdef __cplusplus
}
#endif

#endif /* CC351F44_DBE2_4CB7_9FBA_52E0AC84322E */
