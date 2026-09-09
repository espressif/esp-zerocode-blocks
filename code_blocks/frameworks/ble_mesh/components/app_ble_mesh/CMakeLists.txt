# Keep this in step with the app_ble_mesh CMakeLists the engine emits
# (engine/src/generator.ts). The engine overwrites app_ble_mesh.cpp with the
# real implementation UNCONDITIONALLY but only rewrites this CMakeLists when
# the product has device types — so for a framework-only product THIS file is
# what the generated implementation is compiled against, and `app_driver
# app_config` alone links neither the mesh stack nor the `mesh` console
# command.
#
# `bt` is the component that carries BOTH the BLE host and esp_ble_mesh
# (components/bt/esp_ble_mesh is a subdirectory of it) — leaving it out is the
# undefined-reference-to-esp_ble_mesh_init failure.
idf_component_register(
    SRCS "app_ble_mesh.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES bt nvs_flash app_driver app_config app_console console esp_timer
)
