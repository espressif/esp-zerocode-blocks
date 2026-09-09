#include <sdkconfig.h>
#if CONFIG_SOC_WIFI_SUPPORTED || CONFIG_SOC_WIRELESS_HOST_SUPPORTED
#include <esp_wifi.h>
#include <esp_event.h>
#endif
#include <esp_system.h>
#include <esp_timer.h>
