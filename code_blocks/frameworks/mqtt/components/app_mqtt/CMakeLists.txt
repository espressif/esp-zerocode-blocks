# Keep this in step with the app_mqtt CMakeLists the engine emits
# (engine/src/generator.ts). The engine overwrites app_mqtt.cpp with the real
# implementation UNCONDITIONALLY but only rewrites this CMakeLists when the
# product has device types — so for a framework-only product THIS file is what
# the generated implementation is compiled against, and `app_driver app_config`
# alone cannot compile it (mqtt_client.h, cJSON for the Home Assistant discovery
# payloads, the console commands that set the broker URI).
idf_component_register(
    SRCS "app_mqtt.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES espressif__mqtt espressif__cjson esp_wifi esp_netif esp_event nvs_flash app_driver app_config app_console console
)
