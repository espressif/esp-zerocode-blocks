#include <esp_timer.h>
#if SOC_PCNT_SUPPORTED
#include <driver/pulse_cnt.h>
#else
#include <driver/gpio.h>
#include <esp_attr.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#endif
