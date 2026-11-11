# ESP-IDF 6 emptied the legacy `driver` component: it now carries ONLY i2c,
# touch_sensor and twai. gpio, ledc, i2s, spi, pcnt, rmt, uart and gptimer all
# became separate `esp_driver_*` components that `driver` no longer re-exports.
#
# They are listed explicitly here, rather than left to each block's
# requires.cmake, to keep the contract for product code identical to what it
# was on IDF 5: a generated tree where `#include <driver/ledc.h>` simply works,
# with no CMake edit per header. `driver` stays for the legacy i2c the sensor
# blocks use.
#
# WHOLE_ARCHIVE: blocks define WEAK hooks in the generated control.cpp
# (board_button's <prefix>_on_double_click, …) that the product overrides with
# a strong definition in a file of its own. Linked as an ordinary archive, a
# member that holds ONLY such an override is never pulled in — control.o
# already satisfies the reference — so the override links cleanly and silently
# does nothing. Whole-archive links every file the product put here.
# Every .c / .cpp in this directory is compiled, so a new source file needs
# no edit here (CONFIGURE_DEPENDS re-globs at build time). IDF's requirement
# scan runs this file in script mode, where the flag is refused and no
# sources are needed.
set(app_logic_srcs)
if(NOT CMAKE_SCRIPT_MODE_FILE)
    file(GLOB app_logic_srcs CONFIGURE_DEPENDS
         "${CMAKE_CURRENT_LIST_DIR}/*.c" "${CMAKE_CURRENT_LIST_DIR}/*.cpp")
endif()

# Components the PRODUCT's own code needs beyond these go in product.cmake,
# beside this file: it sets the variable cleared below to the list, e.g. to
# esp_http_client. The engine never writes product.cmake; it rewrites this
# file on every generate, so an edit here is lost. (No requirement line is
# spelled out in this comment: an older engine merged block requirements
# into the first one it found, comments included.) The include is outside the
# script-mode guard on purpose: IDF's requirement scan must see the list too.
# The glob makes a configured build re-run CMake when the file appears or goes
# (an included file's own edits are tracked by CMake already).
set(PRODUCT_PRIV_REQUIRES)
if(NOT CMAKE_SCRIPT_MODE_FILE)
    file(GLOB product_cmake CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/product.cmake")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/product.cmake" OPTIONAL)

idf_component_register(
    SRCS ${app_logic_srcs}
    INCLUDE_DIRS "include"
    WHOLE_ARCHIVE
    PRIV_REQUIRES app_driver app_config app_utils nvs_flash esp_timer espressif__button
                  driver esp_driver_gpio esp_driver_ledc esp_driver_i2s esp_driver_spi
                  esp_driver_pcnt esp_driver_rmt esp_driver_uart esp_driver_gptimer
                  ${PRODUCT_PRIV_REQUIRES}
)
