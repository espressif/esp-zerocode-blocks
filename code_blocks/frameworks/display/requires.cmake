# Extra PRIV_REQUIRES this block adds to generated components.
list(APPEND main_PRIV_REQUIRES app_display)
# Product code reaches the display framework (app_display.h: app_display_on_touch)
# from components/app_logic.
list(APPEND app_logic_PRIV_REQUIRES app_display)
