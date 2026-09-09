{
    // Share the iot_button device for this GPIO (esp_iot_button v4.x binds one
    // device per GPIO). A control button on the same pin owns single-click; we
    // just add a long-press callback. The hold time is set PER-CALLBACK via
    // event_args.long_press.press_time, so it's independent of any other
    // consumer's timing and doesn't depend on who created the device.
    s_{{prefix_lc}}_fr_btn = app_button_get_or_create({{prefix}}_FR_GPIO, {{prefix}}_FR_LEVEL, {{prefix}}_FR_HOLD_MS);
    if (s_{{prefix_lc}}_fr_btn) {
        button_event_args_t {{prefix_lc}}_lp_args = {};
        {{prefix_lc}}_lp_args.long_press.press_time = {{prefix}}_FR_HOLD_MS;
        iot_button_register_cb(s_{{prefix_lc}}_fr_btn, BUTTON_LONG_PRESS_START, &{{prefix_lc}}_lp_args, {{prefix_lc}}_factory_reset_cb, NULL);
        ESP_LOGI(TAG, "{{prefix_lc}}: factory-reset armed — hold GPIO %d for %u ms", {{prefix}}_FR_GPIO, {{prefix}}_FR_HOLD_MS);
    } else {
        ESP_LOGE(TAG, "{{prefix_lc}}: factory-reset button init failed");
    }
}
