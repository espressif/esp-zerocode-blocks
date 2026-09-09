case {{cfg.position_param}}: {
    /* Bang-bang: drive toward target until limit switch trips. App code should
     * monitor the limit GPIOs and call {{prefix_lc}}_motor_off() when the appropriate
     * limit fires. This driver only kicks the motor in the right direction. */
    s_{{prefix_lc}}_target = val.u16;
    if (val.u16 > 5000) {
        /* Open */
        gpio_set_level((gpio_num_t){{prefix}}_DOWN_GPIO, 0);
        gpio_set_level((gpio_num_t){{prefix}}_UP_GPIO, 1);
    } else {
        /* Close */
        gpio_set_level((gpio_num_t){{prefix}}_UP_GPIO, 0);
        gpio_set_level((gpio_num_t){{prefix}}_DOWN_GPIO, 1);
    }
    ESP_LOGI(TAG, "Curtain {{prefix_lc}}: target=%u%% (limits will halt motion)", val.u16 / 100U);
    return ESP_OK;
}
