
idf_component_register(
    SRCS logger_config.c
    INCLUDE_DIRS "include"
    REQUIRES logger_common
    PRIV_REQUIRES strutil
)

if(CONFIG_GPS_LOG_ENABLED)
    target_link_libraries(${COMPONENT_LIB} PUBLIC idf::gps_log)
endif()
if(CONFIG_UBLOX_ENABLED)
    target_link_libraries(${COMPONENT_LIB} PUBLIC idf::logger_ubx)
endif()
if(CONFIG_LOGGER_VFS_ENABLED)
    target_link_libraries(${COMPONENT_LIB} PRIVATE idf::logger_vfs)
endif()