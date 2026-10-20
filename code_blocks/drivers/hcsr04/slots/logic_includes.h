/* The poll callback lives in app_logic, which gets only the union of the
   product's logic_includes — driver/gpio.h in driver_includes.h reaches
   app_driver.cpp, not here, so without it gpio_set_level is undeclared
   unless some other block happens to include it. */
#include <driver/gpio.h>
#include <esp_rom_sys.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
