# Keep this in step with the app_webui CMakeLists the engine emits
# (engine/src/generator.ts). The engine overwrites app_webui.cpp with the real
# implementation UNCONDITIONALLY but only rewrites this CMakeLists when the
# product has device types — so for a framework-only product THIS file is what
# the generated implementation is compiled against, and declaring only
# `app_driver app_config` made the apply_template baseline build fail on
# `esp_event.h: No such file or directory` before code_writing had touched
# anything (paper-round-msmzogkb, 2026-08-10 — the agent then repaired it
# per-product, so the catalog stayed broken for the next one).
idf_component_register(
    SRCS "app_webui.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_http_server esp_wifi esp_netif esp_event nvs_flash espressif__mdns app_driver app_config
)
