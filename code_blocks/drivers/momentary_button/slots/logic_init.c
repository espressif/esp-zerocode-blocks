{
    // Shared button registry: one iot_button device per GPIO (see drivers/button).
    s_{{prefix_lc}}_button = app_button_get_or_create({{prefix}}_BUTTON_GPIO, {{prefix}}_BUTTON_ACTIVE_LEVEL, 0);
    if (!s_{{prefix_lc}}_button) {
        ESP_LOGE(TAG, "Failed to create momentary button {{prefix_lc}}");
    } else {
        iot_button_register_cb(s_{{prefix_lc}}_button, BUTTON_SINGLE_CLICK, NULL, {{prefix_lc}}_button_cb, NULL);
        ESP_LOGI(TAG, "Momentary button {{prefix_lc}} initialized (GPIO %d)", {{prefix}}_BUTTON_GPIO);
    }
}
