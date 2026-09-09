# main calls app_tflite_init(); app_logic is where drivers/tflite_classifier's
# slots land, and they include app_tflite.h.
list(APPEND main_PRIV_REQUIRES app_tflite)
list(APPEND app_logic_PRIV_REQUIRES app_tflite)
