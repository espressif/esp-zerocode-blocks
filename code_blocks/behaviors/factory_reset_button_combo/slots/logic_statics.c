static int64_t s_{{prefix_lc}}_combo_start_us = 0;

static void {{prefix_lc}}_combo_poll(void *arg)
{
    int a = gpio_get_level((gpio_num_t){{prefix}}_FR_GPIO_A);
    int b = gpio_get_level((gpio_num_t){{prefix}}_FR_GPIO_B);
    bool both = (a == {{prefix}}_FR_LEVEL) && (b == {{prefix}}_FR_LEVEL);
    if (!both) {
        s_{{prefix_lc}}_combo_start_us = 0;
        return;
    }
    int64_t now = esp_timer_get_time();
    if (s_{{prefix_lc}}_combo_start_us == 0) s_{{prefix_lc}}_combo_start_us = now;
    if ((now - s_{{prefix_lc}}_combo_start_us) >= ((int64_t){{prefix}}_FR_HOLD_MS * 1000)) {
        ESP_LOGW(TAG, "{{prefix_lc}}: button combo held — erasing NVS and rebooting");
        nvs_flash_erase();
        esp_restart();
    }
}
