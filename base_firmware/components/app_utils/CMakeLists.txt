idf_component_register(
    SRCS "app_utils.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_ledc
    PRIV_REQUIRES nvs_flash
)
