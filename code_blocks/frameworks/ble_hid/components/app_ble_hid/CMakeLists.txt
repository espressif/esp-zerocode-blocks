# Keep this in step with the app_ble_hid CMakeLists the engine emits
# (engine/src/generator.ts). The engine overwrites app_ble_hid.cpp with the real
# implementation UNCONDITIONALLY but only rewrites this CMakeLists when the
# product has device types — so for a framework-only product THIS file is what
# the generated implementation is compiled against. When it declared just
# `app_driver app_config`, the apply_template baseline build failed before the
# agent had written a line (couch-commander-msmzp0aw, 2026-08-10: 3 wasted
# compile rounds, and the run only escaped the non-convergence guard on a human
# nudge). Declaring the real dependency set here makes both paths correct.
#
# ble_hid_gap.c carries the GAP/advertising/security boilerplate this framework's
# block.yml documents as part of the component; out of SRCS, none of it compiles.
idf_component_register(
    SRCS "app_ble_hid.cpp" "ble_hid_gap.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_hid bt esp_event nvs_flash app_driver app_config
)
