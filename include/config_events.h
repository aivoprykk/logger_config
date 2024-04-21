#ifndef E4637772_1304_4CBE_8AE1_F6191216D7FD
#define E4637772_1304_4CBE_8AE1_F6191216D7FD

#include "esp_event.h"

#ifdef __cplusplus
extern "C" {
#endif

ESP_EVENT_DECLARE_BASE(LOGGER_CONFIG_EVENT);

enum {
    LOGGER_CONFIG_EVENT_CONFIG_INIT_DONE,
    LOGGER_CONFIG_EVENT_CONFIG_LOAD_DONE,
    LOGGER_CONFIG_EVENT_CONFIG_LOAD_FAIL,
    LOGGER_CONFIG_EVENT_CONFIG_SAVE_DONE,
    LOGGER_CONFIG_EVENT_CONFIG_SAVE_FAIL,
};

#ifdef __cplusplus
}
#endif

#endif /* E4637772_1304_4CBE_8AE1_F6191216D7FD */
