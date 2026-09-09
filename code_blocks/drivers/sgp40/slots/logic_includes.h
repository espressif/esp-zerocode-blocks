/* vTaskDelay / pdMS_TO_TICKS. Explicit because driver/i2c_master.h does NOT
   pull FreeRTOS in the way the legacy driver/i2c.h did (via
   driver/i2c_types_legacy.h, whose timeouts are TickType_t). Blocks that
   relied on that transitive include compiled only in products where some
   other block happened to include it. */
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/i2c_master.h>
#include <esp_timer.h>
