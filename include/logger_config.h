#ifndef CC351F44_DBE2_4CB7_9FBA_52E0AC84322E
#define CC351F44_DBE2_4CB7_9FBA_52E0AC84322E

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <json.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const char *config_item_names;
extern const char * const config_speed_field_item_names[];
extern const char * const config_stat_screen_item_names[];
extern const char * const config_screen_item_names[];
extern const char * const config_gps_item_names[];

typedef struct logger_config_item_s {
    const char * name;
    int pos;
    uint32_t value;
    const char *desc;
} logger_config_item_t;

typedef struct logger_config_gps_s {
    uint8_t gnss;             // default setting 2 GNSS, GPS & GLONAS
    uint8_t sample_rate;      // gps_rate in Hz, 1, 5 or 10Hz !!!
    uint8_t speed_unit;       // 0 = m/s, 1 = km/h, 2 = knots
    uint8_t log_txt;          // switchinf off .txt files
    uint8_t log_ubx;          // log to .ubx
    uint8_t log_sbp;          // log to .sbp
    uint8_t log_gpy;          // log to .gps
    uint8_t log_gpx;          // log to .gpx
    uint8_t log_ubx_nav_sat;  // log nav sat msg to .ubx
    uint8_t dynamic_model;    // choice for dynamic model "Sea",if 0 model "portable" is used !!
    uint16_t bar_length;      // choice for bar indicator for length of run in m (nautical mile)
} logger_config_gps_t;
#define L_CONFIG_GPS_FIELDS sizeof(struct logger_config_gps_s)
#define LOGGER_CONFIG_GPS_DEFAULTS() { \
    .gnss = 111, \
    .sample_rate = 10, \
    .speed_unit = 1, \
    .log_txt = true, \
    .log_ubx = true, \
    .log_sbp = false, \
    .log_gpy = false, \
    .log_gpx = false, \
    .log_ubx_nav_sat = false, \
    .dynamic_model = 0, \
    .bar_length = 1852, \
}

typedef struct logger_config_speed_field_s {
    uint8_t dynamic;
    uint8_t stat_10_sec;
    uint8_t stat_alpha;
    uint8_t stat_1852_m;
    uint8_t stat_dist_500m;
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

typedef struct logger_config_stat_screens_s {
    uint8_t stat_10_sec;
    uint8_t stat_2_sec;
    uint8_t stat_250_m;
    uint8_t stat_500_m;
    uint8_t stat_1852_m;
    uint8_t stat_alfa;
    uint8_t stat_avg_10sec;
    uint8_t stat_stat1;
    uint8_t stat_avg_a500;
} logger_config_stat_screens_t;
#define L_CONFIG_STAT_FIELDS sizeof(struct logger_config_stat_screens_s)
#define LOGGER_CONFIG_STAT_SCREENS_DEFAULTS() { \
    .stat_10_sec = 1, \
    .stat_2_sec = 1, \
    .stat_250_m = 1, \
    .stat_500_m = 1, \
    .stat_1852_m = 1, \
    .stat_alfa = 0, \
    .stat_avg_10sec = 1, \
    .stat_stat1 = 1, \
    .stat_avg_a500 = 0, \
}

typedef struct logger_config_screen_s {
    uint8_t speed_field;             // choice for first field in speed screen !!!
    uint8_t speed_large_font;        // fonts on the first line are bigger, actual speed font is smaller
    uint8_t stat_screens_time;       // time between switching stat_screens
    uint8_t board_logo;
    uint8_t sail_logo;
    uint8_t screen_rotation;
    uint8_t screen_no_auto_refresh;
    uint8_t stat_speed;       // max speed in m/s for showing Stat screens
    uint16_t stat_screens;    // choice for stats field when no speed, here stat_screen 1, 2 and 3 will be active
    uint16_t gpio12_screens;  // choice for stats field when gpio12 is activated (pull-up high, low = active)
} logger_config_screen_t;
#define L_CONFIG_SCREEN_FIELDS sizeof(struct logger_config_screen_s)
#define LOGGER_CONFIG_SCREEN_DEFAULTS() { \
    .speed_field = 1, \
    .speed_large_font = 0, \
    .stat_screens_time = 3, \
    .board_logo = 1, \
    .sail_logo = 1, \
    .stat_speed = 1, \
    .screen_rotation = 1, \
    .screen_no_auto_refresh = false, \
    .stat_screens = 255U, \
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

#define LOGGER_CONFIG_FWUPDATE_DEFAULTS() { \
    .update_enabled = true, \
    .channel = FW_UPDATE_CHANNEL_PROD, \
}

#define L_CONFIG_SSID_MAX 4

typedef struct logger_config_wifi_sta_s {
    char ssid[32];        // your SSID
    char password[32];    // your password
} logger_config_wifi_sta_t;

typedef struct logger_config_s {
    logger_config_gps_t gps;
    logger_config_screen_t screen;
    float timezone;           // choice for timedifference in hours with UTC, for Belgium 1 or 2 (summertime)
    uint8_t file_date_time;   // type of filenaming, with MAC adress or datetime
    uint8_t config_fail;
    int8_t  screen_move_offset;
    uint8_t speed_field_count;

    uint16_t archive_days;    // how many days files will be moved to the "Archive" dir
 
    char ubx_file[32];    // your preferred filename
    char sleep_info[32];  // your preferred sleep text

    struct logger_config_fwupdate_c fwupdate;

    struct logger_config_wifi_sta_s wifi_sta[L_CONFIG_SSID_MAX]; // your SSID and password
    char hostname[32];    // your hostname
    void(*config_changed_screen_cb)(const char *name);
} logger_config_t;

#define LOGGER_CONFIG_DEFAULTS() { \
    .gps = LOGGER_CONFIG_GPS_DEFAULTS(), \
    .screen = LOGGER_CONFIG_SCREEN_DEFAULTS(), \
    .file_date_time = 2, \
    .config_fail = 0, \
    .screen_move_offset = 1, \
    .speed_field_count = L_CONFIG_SPEED_FIELDS, \
    .archive_days = 30, \
    .timezone = 2, \
    .ubx_file = "gps", \
    .sleep_info = "ESP GPS", \
    .fwupdate = LOGGER_CONFIG_FWUPDATE_DEFAULTS(), \
    .hostname = "esp-logger", \
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
logger_config_t *config_new();

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
char *config_get(const struct logger_config_s *config, const char *name, char *str, size_t *len, size_t max, uint8_t mode, const uint8_t ublox_hw);

/*
* @brief Set a variable in the configuration
* @param config The configuration to set the variable in
* @param name The name of the variable to set
* @param str The string to set the variable from
* @param force The force to set the variable with
*/
int config_set(logger_config_t *config, JsonNode *root, const char *str, uint8_t force);

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
int config_save_json(struct logger_config_s *config, uint8_t ublox_hw);

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
* @brief Get a JSON string from a configuration
* @param config The configuration to get a JSON string from
* @param sb The string builder to use
* @param str The string to get the JSON string into
*/
char *config_get_json(struct logger_config_s * config, struct strbf_s *sb, const char *str, uint8_t ublox_hw);

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
char *config_encode_json(struct logger_config_s * config, struct strbf_s *sb, uint8_t ublox_hw);

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
int config_save_var(struct logger_config_s *config, const char *json, const char *var, uint8_t ublox_hw);

/*
* @brief Save a variable in the configuration to a file
* @param config The configuration to save the variable from
* @param filename The filename to save the variable to
* @param filename_b The filename to save the variable to
* @param json The JSON string to save the variable from
* @param var The variable to save
*/
int config_save_var_b(struct logger_config_s *config, const char *json, uint8_t ublox_hw);

esp_err_t config_set_screen_cb(logger_config_t * config, void(*cb)(const char *));

logger_config_item_t * get_gps_cfg_item(const logger_config_t *config, int num, logger_config_item_t *item);
int set_gps_cfg_item(logger_config_t *config, int num, uint8_t ublox_hw);
logger_config_item_t * get_stat_screen_cfg_item(const logger_config_t *config, int num, logger_config_item_t *item);
int set_stat_screen_cfg_item(logger_config_t * config, int num, uint8_t ublox_hw);
logger_config_item_t * get_screen_cfg_item(const logger_config_t *config, int num, logger_config_item_t *item);
int set_screen_cfg_item(logger_config_t * config, int num, uint8_t ublox_hw);
logger_config_item_t * get_fw_update_cfg_item(const logger_config_t *config, int num, logger_config_item_t *item);
int set_fw_update_cfg_item(logger_config_t * config, int num, uint8_t ublox_hw);
#ifdef __cplusplus
}
#endif

#endif /* CC351F44_DBE2_4CB7_9FBA_52E0AC84322E */
