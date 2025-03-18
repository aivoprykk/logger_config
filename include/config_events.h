#ifndef E4637772_1304_4CBE_8AE1_F6191216D7FD
#define E4637772_1304_4CBE_8AE1_F6191216D7FD

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_event.h"
#include "logger_common.h"

ESP_EVENT_DECLARE_BASE(LOGGER_CONFIG_EVENT);        // declaration of the LOG_EVENT family

#define LOGGER_CONFIG_EVENT_LIST(l) \
    l(LOGGER_CONFIG_EVENT_INIT_DONE) \
    l(LOGGER_CONFIG_EVENT_LOAD_DONE) \
    l(LOGGER_CONFIG_EVENT_LOAD_FAIL) \
    l(LOGGER_CONFIG_EVENT_SAVE_DONE) \
    l(LOGGER_CONFIG_EVENT_SAVE_FAIL) \
    l(LOGGER_CONFIG_EVENT_CFG_CHANGED) \
    l(LOGGER_CONFIG_EVENT_CFG_SET) \
    l(LOGGER_CONFIG_EVENT_CFG_GET) \
// declaration of the specific events under the LOG_EVENT family
enum {                                       
    LOGGER_CONFIG_EVENT_LIST(ENUM)
};

extern const char * const logger_config_event_strings[];

#ifdef __cplusplus
}
#endif

#endif /* E4637772_1304_4CBE_8AE1_F6191216D7FD */
