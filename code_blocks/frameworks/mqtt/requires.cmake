# Extra PRIV_REQUIRES this block adds to generated components.
list(APPEND main_PRIV_REQUIRES app_mqtt)
# Product code in app_logic calls the product-entity API (app_mqtt.h).
list(APPEND app_logic_PRIV_REQUIRES app_mqtt)
