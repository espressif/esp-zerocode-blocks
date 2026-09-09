case {{cfg.trigger_param}}: {
    /* TRIGGER, not level: any non-zero write buzzes once. The param is not a
       state anyone should read back, so nothing latches it. */
    if (val.u8) {
        ESP_LOGI(TAG, "Buzz {{prefix_lc}} (%d ms)", {{prefix}}_HAPTIC_PULSE_MS);
        {{prefix_lc}}_buzz();
    }
    return ESP_OK;
}
