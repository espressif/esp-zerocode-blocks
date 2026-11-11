# Extra PRIV_REQUIRES this block adds to generated components.
list(APPEND main_PRIV_REQUIRES app_webui)
# Product code reaches the page's server (app_webui_register_handlers,
# app_webui_server) from components/app_logic.
list(APPEND app_logic_PRIV_REQUIRES app_webui)
