# Keep this in step with the app_audio CMakeLists the engine emits
# (engine/src/generator.ts). The engine overwrites app_audio.cpp with the real
# implementation UNCONDITIONALLY but only rewrites this CMakeLists when the
# product has device types — so for a framework-only product THIS file is what
# the generated implementation is compiled against, and it cannot see esp-sr's
# WakeNet/MultiNet headers or esp_srmodel_init() without the dependency below.
idf_component_register(
    SRCS "app_audio.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES espressif__esp-sr app_driver app_config esp_driver_i2s
)
