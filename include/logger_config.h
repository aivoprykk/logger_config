#ifndef CC351F44_DBE2_4CB7_9FBA_52E0AC84322E
#define CC351F44_DBE2_4CB7_9FBA_52E0AC84322E

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <json.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct logger_config_s {
    bool log_txt;          // switchinf off .txt files
    bool log_ubx;          // log to .ubx
    bool log_sbp;          // log to .sbp
    bool log_gpy;          // log to .gps
    bool log_gpx;          // log to .gpx
    bool log_ubx_nav_sat;  // log nav sat msg to .ubx
    
    uint8_t sample_rate;             // gps_rate in Hz, 1, 5 or 10Hz !!!
    uint8_t gnss;                    // default setting 2 GNSS, GPS & GLONAS
    uint8_t speed_field;             // choice for first field in speed screen !!!
    uint8_t speed_large_font;        // fonts on the first line are bigger, actual speed font is smaller
    uint8_t dynamic_model;           // choice for dynamic model "Sea",if 0 model "portable" is used !!
    uint8_t stat_screens_time;       // time between switching stat_screens
    uint8_t stat_screens_persist;    // choice for stats field when no speed, here stat_screen 1, 2 and 3 will be active for resave the config
    uint8_t gpio12_screens_persist;  // choice for stats field when gpio12 is activated (pull-up high, low = active) for resave the config
    uint8_t board_Logo;
    uint8_t sail_Logo;
    uint8_t sleep_off_screen;
    uint8_t stat_speed;       // max speed in m/s for showing Stat screens
    uint8_t file_date_time;   // type of filenaming, with MAC adress or datetime
    uint8_t config_fail;
    int8_t  ublox_type;
    uint8_t speed_field_count;

    uint32_t stat_screens;    // choice for stats field when no speed, here stat_screen 1, 2 and 3 will be active
    uint32_t gpio12_screens;  // choice for stats field when gpio12 is activated (pull-up high, low = active)
    uint16_t bar_length;      // choice for bar indicator for length of run in m (nautical mile)
    uint16_t archive_days;    // how many days files will be moved to the "Archive" dir

    //float cal_bat;          // calibration for read out bat voltage
    float cal_speed;          // conversion m/s to km/h, for knots use 1.944
    float timezone;           // choice for timedifference in hours with UTC, for Belgium 1 or 2 (summertime)
 
    char ubx_file[32];    // your preferred filename
    char sleep_info[32];  // your preferred sleep text
    char ssid[32];        // your SSID
    char password[32];    // your password
} logger_config_t;

#define LOGGER_CONFIG_DEFAULTS() { \
    .log_txt = true, \
    .log_ubx = true, \
    .log_sbp = false, \
    .log_gpy = false, \
    .log_gpx = false, \
    .log_ubx_nav_sat = false, \
    .sample_rate = 5, \
    .gnss = 5, \
    .speed_field = 1, \
    .speed_large_font = 1, \
    .dynamic_model = 0, \
    .stat_screens_time = 3, \
    .stat_screens_persist = 123U, \
    .gpio12_screens_persist = 12U, \
    .board_Logo = 1, \
    .sail_Logo = 1, \
    .sleep_off_screen = 11, \
    .stat_speed = 1, \
    .file_date_time = 2, \
    .config_fail = 0, \
    .ublox_type = 0, \
    .speed_field_count =9, \
    .stat_screens = 123U, \
    .gpio12_screens = 12U, \
    .bar_length = 1852, \
    .archive_days = 30, \
    .cal_speed = 3.6, \
    .timezone = 2, \
    .ubx_file = "gps", \
    .sleep_info = "ESP GPS", \
    .ssid = "", \
    .password = "" \
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
char *config_get(struct logger_config_s *config, const char *name, char *str, size_t *len, size_t max, uint8_t mode, uint8_t ublox_hw);

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
int config_load_json(struct logger_config_s *config, const char *filename, const char *filename_backup);

/*
* @brief Save the configuration to a file
* @param config The configuration to save
* @param filename The filename to save the configuration to
* @param filename_backup The filename to save the configuration to
*/
int config_save_json(struct logger_config_s *config, const char *filename, const char *filename_backup, uint8_t ublox_hw);

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
int config_save_var(struct logger_config_s *config, const char *filename, const char * filename_b, const char *json, const char *var, uint8_t ublox_hw);

/*
* @brief Save a variable in the configuration to a file
* @param config The configuration to save the variable from
* @param filename The filename to save the variable to
* @param filename_b The filename to save the variable to
* @param json The JSON string to save the variable from
* @param var The variable to save
*/
int config_save_var_b(struct logger_config_s *config, const char *filename, const char * filename_b, const char *json, uint8_t ublox_hw);

#ifdef __cplusplus
}
#endif

#endif /* CC351F44_DBE2_4CB7_9FBA_52E0AC84322E */