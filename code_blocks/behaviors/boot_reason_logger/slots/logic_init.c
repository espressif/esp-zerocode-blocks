{
    esp_reset_reason_t {{prefix_lc}}_r = esp_reset_reason();
    const char *{{prefix_lc}}_name = "unknown";
    switch ({{prefix_lc}}_r) {
        case ESP_RST_POWERON:   {{prefix_lc}}_name = "power-on";       break;
        case ESP_RST_EXT:       {{prefix_lc}}_name = "external";       break;
        case ESP_RST_SW:        {{prefix_lc}}_name = "software";       break;
        case ESP_RST_PANIC:     {{prefix_lc}}_name = "panic";          break;
        case ESP_RST_INT_WDT:   {{prefix_lc}}_name = "int-watchdog";   break;
        case ESP_RST_TASK_WDT:  {{prefix_lc}}_name = "task-watchdog";  break;
        case ESP_RST_WDT:       {{prefix_lc}}_name = "other-watchdog";break;
        case ESP_RST_DEEPSLEEP: {{prefix_lc}}_name = "deep-sleep-wake";break;
        case ESP_RST_BROWNOUT:  {{prefix_lc}}_name = "brownout";       break;
        case ESP_RST_SDIO:      {{prefix_lc}}_name = "sdio";           break;
        default: break;
    }
    ESP_LOGI(TAG, "Boot reason: %s (%d)", {{prefix_lc}}_name, (int){{prefix_lc}}_r);
}
