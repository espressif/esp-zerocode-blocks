# Extra PRIV_REQUIRES this block adds to generated components.
# esp_driver_pcnt compiles to nothing on chips without the peripheral; the
# GPIO-ISR fallback path needs esp_driver_gpio explicitly.
list(APPEND app_logic_PRIV_REQUIRES esp_driver_pcnt esp_driver_gpio)
