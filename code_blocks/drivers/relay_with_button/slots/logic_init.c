{
    // Shared button registry: one iot_button device per GPIO (see drivers/button).
    s_{{prefix_lc}}_btn = app_button_get_or_create({{prefix}}_BUTTON_GPIO, {{prefix}}_BUTTON_ACTIVE_LEVEL, 0);
    if (s_{{prefix_lc}}_btn) {
        iot_button_register_cb(s_{{prefix_lc}}_btn, BUTTON_SINGLE_CLICK, NULL, {{prefix_lc}}_btn_cb, NULL);
        iot_button_register_cb(s_{{prefix_lc}}_btn, BUTTON_PRESS_DOWN, NULL, {{prefix_lc}}_press_down_cb, NULL);
        iot_button_register_cb(s_{{prefix_lc}}_btn, BUTTON_PRESS_UP,   NULL, {{prefix_lc}}_press_up_cb, NULL);
        iot_button_register_cb(s_{{prefix_lc}}_btn, BUTTON_PRESS_END,  NULL, {{prefix_lc}}_press_end_cb, NULL);
    } else {
        ESP_LOGE(TAG, "Failed to create button for {{prefix_lc}}");
    }
}
