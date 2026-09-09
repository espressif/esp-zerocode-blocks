{
    // Shared button registry: one iot_button device per GPIO. Get (or create
    // on first use) the handle, then register THIS consumer's callback on it.
    // Lets a control button + long-press factory reset share one GPIO safely.
    s_{{prefix_lc}}_button = app_button_get_or_create({{prefix}}_BUTTON_GPIO, {{prefix}}_BUTTON_ACTIVE_LEVEL, 0);
    if (!s_{{prefix_lc}}_button) {
        ESP_LOGE(TAG, "Failed to create button {{prefix_lc}}");
    } else {
        iot_button_register_cb(s_{{prefix_lc}}_button, BUTTON_SINGLE_CLICK, NULL, {{prefix_lc}}_button_cb, NULL);
        ESP_LOGI(TAG, "Button {{prefix_lc}} initialized (GPIO %d)", {{prefix}}_BUTTON_GPIO);
    }
}
