# ESP-IDF 6 emptied the legacy `driver` component: it now carries ONLY i2c,
# touch_sensor and twai. gpio, ledc, i2s, spi, pcnt, rmt, uart and gptimer all
# became separate `esp_driver_*` components that `driver` no longer re-exports.
#
# They are listed explicitly here, rather than left to each block's
# requires.cmake, to keep the AGENT-FACING contract identical to what it was on
# IDF 5: a generated tree where `#include <driver/ledc.h>` simply works. The
# agents write C at runtime against these headers, and making each one a CMake
# dependency puzzle would spend their budget debugging the build system instead
# of the product. `driver` stays for the legacy i2c the sensor blocks use.
idf_component_register(
    SRCS "app_logic.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES app_driver app_config app_utils nvs_flash esp_timer espressif__button
                  driver esp_driver_gpio esp_driver_ledc esp_driver_i2s esp_driver_spi
                  esp_driver_pcnt esp_driver_rmt esp_driver_uart esp_driver_gptimer
)
