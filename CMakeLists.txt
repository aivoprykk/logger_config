
idf_component_register(
    SRCS logger_config.c
    INCLUDE_DIRS "include"
    REQUIRES ccan_json
    PRIV_REQUIRES logger_common logger_vfs logger_str logger_ubx
)
