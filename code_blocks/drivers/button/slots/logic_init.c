{
    // Shared button registry: one iot_button device per GPIO. Get (or create
    // on first use) the handle, then register THIS consumer's callback on it.
    // Lets a control button + long-press factory reset share one GPIO safely.
    s_{{prefix_lc}}_button = app_button_get_or_create({{prefix}}_BUTTON_GPIO, {{prefix}}_BUTTON_ACTIVE_LEVEL, 0);
    if (!s_{{prefix_lc}}_button) {
        ESP_LOGE(TAG, "Failed to create button {{prefix_lc}}");
    } else {
        iot_button_register_cb(s_{{prefix_lc}}_button, BUTTON_SINGLE_CLICK, NULL, {{prefix_lc}}_button_cb, NULL);
        iot_button_register_cb(s_{{prefix_lc}}_button, BUTTON_PRESS_DOWN, NULL, {{prefix_lc}}_press_down_cb, NULL);
        iot_button_register_cb(s_{{prefix_lc}}_button, BUTTON_PRESS_UP,   NULL, {{prefix_lc}}_press_up_cb, NULL);
        iot_button_register_cb(s_{{prefix_lc}}_button, BUTTON_PRESS_END,  NULL, {{prefix_lc}}_press_end_cb, NULL);
        ESP_LOGI(TAG, "Button {{prefix_lc}} initialized (GPIO %d)", {{prefix}}_BUTTON_GPIO);
        if ({{prefix_lc}}_woke_device() &&
            gpio_get_level((gpio_num_t){{prefix}}_BUTTON_GPIO) != {{prefix}}_BUTTON_ACTIVE_LEVEL) {
            ESP_LOGI(TAG, "Button {{prefix_lc}} woke the device — delivering that press");
{{#if cfg.press_param}}            app_driver_param_val_t {{prefix_lc}}_wv = { .u8 = 1 };
            app_driver_fire_event({{cfg.press_param}}, {{prefix_lc}}_wv, APP_DRIVER_SOURCE_LOCAL);
{{/if}}            {{prefix_lc}}_toggle();
        }
    }
}
