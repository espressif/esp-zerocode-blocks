# Keep this in step with the app_webui CMakeLists the engine emits
# (engine/src/generator.ts). The engine overwrites app_webui.cpp with the real
# implementation UNCONDITIONALLY but only rewrites this CMakeLists when the
# product has device types — so for a framework-only product THIS file is what
# the generated implementation is compiled against: declaring only
# `app_driver app_config` here fails a framework-only product's first build on
# `esp_event.h: No such file or directory`.
idf_component_register(
    SRCS "app_webui.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server
    PRIV_REQUIRES esp_wifi esp_netif esp_event nvs_flash espressif__mdns app_driver app_config
)
