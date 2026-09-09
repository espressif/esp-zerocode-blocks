static void {{prefix_lc}}_hc_poll_cb(void *arg)
{
    gpio_set_level((gpio_num_t){{prefix}}_HC_TRIG_GPIO, 0);
    esp_rom_delay_us(2);
    gpio_set_level((gpio_num_t){{prefix}}_HC_TRIG_GPIO, 1);
    esp_rom_delay_us(10);
    gpio_set_level((gpio_num_t){{prefix}}_HC_TRIG_GPIO, 0);
    int64_t {{prefix_lc}}_t0 = esp_timer_get_time();
    while (gpio_get_level((gpio_num_t){{prefix}}_HC_ECHO_GPIO) == 0) {
        if ((esp_timer_get_time() - {{prefix_lc}}_t0) > 30000) return; /* timeout 30 ms */
    }
    int64_t {{prefix_lc}}_rise = esp_timer_get_time();
    while (gpio_get_level((gpio_num_t){{prefix}}_HC_ECHO_GPIO) == 1) {
        if ((esp_timer_get_time() - {{prefix_lc}}_rise) > 30000) return;
    }
    int64_t {{prefix_lc}}_fall = esp_timer_get_time();
    uint32_t dur_us = (uint32_t)({{prefix_lc}}_fall - {{prefix_lc}}_rise);
    /* mm = us * 343 / 2000 ≈ us * 343 / 2000 */
    uint32_t mm = (dur_us * 343U) / 2000U;
    if (mm > 0xFFFF) mm = 0xFFFF;
    app_driver_param_val_t v = { .u16 = (uint16_t)mm };
    app_driver_set_param({{cfg.distance_mm_param}}, v, APP_DRIVER_SOURCE_LOCAL);
}
