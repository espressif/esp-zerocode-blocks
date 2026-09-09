{
    led_indicator_gpio_config_t {{prefix_lc}}_gpio_cfg = {
        .is_active_level_high = (bool){{prefix}}_STATUS_LED_LEVEL,
        .gpio_num = {{prefix}}_STATUS_LED_GPIO,
    };
    /* led_indicator 2.x replaced the one-shot led_indicator_create(cfg) with a
       per-device-type constructor that returns esp_err_t and writes the handle
       out. `mode` and the union of per-type config pointers are gone with it:
       the device type IS the function now. led_indicator_config_t keeps only
       the blink lists, and NULL blink_lists means "use the defaults", which is
       what this block always wanted. */
    led_indicator_config_t {{prefix_lc}}_led_cfg = {
        .blink_lists = NULL,
        .blink_list_num = 0,
    };
    if (led_indicator_new_gpio_device(&{{prefix_lc}}_led_cfg, &{{prefix_lc}}_gpio_cfg,
                                      &s_{{prefix_lc}}_led) != ESP_OK || !s_{{prefix_lc}}_led) {
        ESP_LOGE(TAG, "Failed to create status LED {{prefix_lc}}");
    } else {
        const esp_timer_create_args_t {{prefix_lc}}_tmr_args = {
            .callback = {{prefix_lc}}_blink_timer_cb,
            .arg = NULL,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "{{prefix_lc}}_led",
            .skip_unhandled_events = false,
        };
        esp_timer_create(&{{prefix_lc}}_tmr_args, &s_{{prefix_lc}}_blink_timer);
        ESP_LOGI(TAG, "Status LED {{prefix_lc}} initialized (GPIO %d)", {{prefix}}_STATUS_LED_GPIO);
    }
}
