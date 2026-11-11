# Keep this in step with the app_matter CMakeLists the engine emits
# (engine/src/generator.ts). The engine overwrites both files for every Matter
# product; this copy is what an older engine, which rewrote the CMakeLists only
# for products with device types, compiles a framework-only product against.
#
# REQUIRES (not PRIV_REQUIRES): esp_matter types appear in this component's
# public header, so dependents need them on their include path too.
idf_component_register(
    SRCS "app_matter.cpp"
    INCLUDE_DIRS "include"
    REQUIRES espressif__esp_matter
    PRIV_REQUIRES app_driver app_config app_utils
)
