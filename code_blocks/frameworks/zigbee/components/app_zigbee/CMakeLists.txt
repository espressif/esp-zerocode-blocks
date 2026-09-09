# Keep this in step with the app_zigbee CMakeLists the engine emits
# (engine/src/generator.ts). The engine overwrites app_zigbee.cpp with the real
# implementation UNCONDITIONALLY but only rewrites this CMakeLists when the
# product has device types — so for a framework-only product THIS file is what
# the generated implementation is compiled against, and `app_driver app_config`
# alone cannot compile the ZCZR router or the factory-reset path that
# erases Zigbee NVRAM.
idf_component_register(
    SRCS "app_zigbee.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES espressif__esp-zigbee-lib app_driver app_config app_console console app_update esp_partition esp_timer
)
