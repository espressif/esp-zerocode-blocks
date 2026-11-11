#include <sdkconfig.h>
#include <string.h>
#include <stdio.h>
#include <esp_console.h>
#include <app_console.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_timer.h>
#include <nvs.h>
#if CONFIG_SOC_WIFI_SUPPORTED || CONFIG_SOC_WIRELESS_HOST_SUPPORTED
#include <esp_wifi.h>
#endif
