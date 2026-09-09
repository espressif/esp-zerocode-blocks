idf_component_register(
    SRCS "app_console.cpp"
    INCLUDE_DIRS "include"
    REQUIRES console nvs_flash
    PRIV_REQUIRES app_config app_utils app_driver
)
