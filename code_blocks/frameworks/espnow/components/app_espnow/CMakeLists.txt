# Keep this in step with the app_espnow CMakeLists the engine emits
# (engine/src/generator.ts). The engine overwrites app_espnow.cpp with the real
# implementation UNCONDITIONALLY but only rewrites this CMakeLists when the
# product has device types — so for a framework-only product THIS file is what
# the generated implementation is compiled against, and declaring only
# `app_driver app_config` failed the apply_template baseline build on
# `esp_event.h: No such file or directory` (shed-buzzer-msn1iaek, 2026-08-10).
#
# Note esp_wifi provides BOTH the Wi-Fi driver and esp_now.h — there is no
# separate ESP-NOW component, in-tree or managed. The build-error advisor sent
# that run after `espressif/esp-now` instead, which cost another failed round.
idf_component_register(
    SRCS "app_espnow.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_wifi esp_netif esp_event nvs_flash esp_timer console app_driver app_config
)
