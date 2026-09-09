#include <esp_matter.h>
/* vTaskDelay / pdMS_TO_TICKS in matter_statics (the identify blink task).
   Explicit because esp_matter.h is not a FreeRTOS header: this block compiled
   only in products where something else in app_matter.cpp happened to bring
   the task API in. Same latent gap the I2C sensors closed. */
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
