# Keep this in step with the app_display CMakeLists the engine emits
# (engine/src/generator.ts). The engine overwrites app_display.cpp with the real
# implementation UNCONDITIONALLY but only rewrites this CMakeLists when the
# product has device types — so for a framework-only product THIS file is what
# the generated implementation is compiled against, and `app_driver app_config`
# alone cannot compile an LVGL panel.
#
# lvgl and esp_lvgl_port are MANAGED dependencies (see this block's
# idf_component.yml); named here in their component-manager form because that is
# what the component is actually called in the build.
idf_component_register(
    SRCS "app_display.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES lvgl__lvgl espressif__esp_lvgl_port esp_lcd app_driver app_config
)
