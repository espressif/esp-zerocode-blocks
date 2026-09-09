# Keep this in step with the app_matter CMakeLists the engine emits
# (engine/src/generator.ts). The engine overwrites app_matter.cpp with the real
# implementation UNCONDITIONALLY but only rewrites this CMakeLists when the
# product has device types — so for a framework-only product THIS file is what
# the generated implementation is compiled against, and it cannot see
# esp_matter.h without the dependency below.
#
# REQUIRES (not PRIV_REQUIRES): esp_matter types appear in this component's
# public header, so dependents need them on their include path too.
idf_component_register(
    SRCS "app_matter.cpp"
    INCLUDE_DIRS "include"
    REQUIRES espressif__esp_matter
    PRIV_REQUIRES app_driver app_config
)
