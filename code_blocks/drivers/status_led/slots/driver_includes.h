#include <led_indicator.h>
/* led_indicator 2.x split the per-device-type config structs out of the main
   header: led_indicator_gpio_config_t and led_indicator_new_gpio_device() live
   here, not in led_indicator.h. */
#include <led_indicator_gpio.h>
#include <esp_timer.h>
