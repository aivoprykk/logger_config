#ifndef FF1FE37F_2A63_46BE_9691_8B160D95C4BC
#define FF1FE37F_2A63_46BE_9691_8B160D95C4BC

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <string.h>

#include "config_events.h"

#include "sdkconfig.h"
#if (defined(CONFIG_LOGGER_USE_GLOBAL_LOG_LEVEL) && CONFIG_LOGGER_GLOBAL_LOG_LEVEL < CONFIG_LOGGER_CONFIG_LOG_LEVEL)
#define C_LOG_LEVEL CONFIG_LOGGER_GLOBAL_LOG_LEVEL
#else
#define C_LOG_LEVEL CONFIG_LOGGER_CONFIG_LOG_LEVEL
#endif
#include "common_log.h"

#ifdef __cplusplus
}
#endif

#endif /* FF1FE37F_2A63_46BE_9691_8B160D95C4BC */
