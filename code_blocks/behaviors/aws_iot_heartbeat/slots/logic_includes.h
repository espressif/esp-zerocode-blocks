#include <stdio.h>
#include <string.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "app_aws_iot.h"
/* The framework puts Espressif's native JSON pair on app_logic's path
 * (frameworks/aws_iot/requires.cmake). json_generator writes into a buffer
 * you own; json_parser reads a text of known length without copying it. */
#include <json_generator.h>
#include <json_parser.h>
