{
    // Shared button registry: one iot_button device per GPIO (see drivers/button).
    // Pass our long-press threshold as the device default; the long-press
    // callback also passes it per-callback via event_args so it's correct even
    // when this GPIO is shared with another long-press consumer.
    s_{{prefix_lc}}_btn = app_button_get_or_create({{prefix}}_BUTTON_GPIO, {{prefix}}_BUTTON_ACTIVE_LEVEL, {{prefix}}_BUTTON_LONG_PRESS_MS);
    if (!s_{{prefix_lc}}_btn) {
        ESP_LOGE(TAG, "Failed to create multi-event button {{prefix_lc}}");
    } else {
        button_event_args_t {{prefix_lc}}_lp_args = {};
        {{prefix_lc}}_lp_args.long_press.press_time = {{prefix}}_BUTTON_LONG_PRESS_MS;
        iot_button_register_cb(s_{{prefix_lc}}_btn, BUTTON_SINGLE_CLICK,    NULL, {{prefix_lc}}_single_click_cb, NULL);
        iot_button_register_cb(s_{{prefix_lc}}_btn, BUTTON_DOUBLE_CLICK,    NULL, {{prefix_lc}}_double_click_cb, NULL);
        iot_button_register_cb(s_{{prefix_lc}}_btn, BUTTON_LONG_PRESS_START, &{{prefix_lc}}_lp_args, {{prefix_lc}}_long_press_cb, NULL);
        ESP_LOGI(TAG, "Multi-event button {{prefix_lc}} initialized (GPIO %d)", {{prefix}}_BUTTON_GPIO);
    }
}
