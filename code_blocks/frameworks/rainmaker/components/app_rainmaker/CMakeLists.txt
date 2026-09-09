# Keep this in step with the app_rainmaker CMakeLists the engine emits
# (engine/src/generator.ts). The engine overwrites app_rainmaker.cpp with the
# real implementation UNCONDITIONALLY but only rewrites this CMakeLists when the
# product has device types — so for a framework-only product THIS file is what
# the generated implementation is compiled against, and `app_driver app_config`
# alone cannot compile the RainMaker node + its provisioning path.
idf_component_register(
    SRCS "app_rainmaker.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES espressif__esp_rainmaker espressif__rmaker_app_network espressif__network_provisioning esp_event app_driver app_config
)
