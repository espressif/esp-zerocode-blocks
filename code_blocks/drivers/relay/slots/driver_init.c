{
    /* Write the INACTIVE level into the output register BEFORE switching the pin
     * to output. gpio_config() starts driving immediately, and the register's
     * reset value is 0 — so on an active-LOW relay the old order (config, then
     * deassert) energized the load for the microseconds in between. On a garage
     * door that is a pulse to the opener on every boot. Setting the level first
     * means the pin's first driven edge is already the safe one; the repeat after
     * config is free and keeps this correct if a chip ever ignores the early
     * write. */
    gpio_set_level((gpio_num_t){{prefix}}_RELAY_GPIO, !{{prefix}}_RELAY_ACTIVE_LEVEL);
    /* Zero-init + assign instead of a designated initializer: newer IDF adds
     * fields to gpio_config_t (6.2: hys_ctrl_mode) and the build runs with
     * -Werror=missing-field-initializers. */
    gpio_config_t {{prefix_lc}}_relay_conf = {};
    {{prefix_lc}}_relay_conf.pin_bit_mask = (1ULL << {{prefix}}_RELAY_GPIO);
    {{prefix_lc}}_relay_conf.mode = GPIO_MODE_OUTPUT;
    {{prefix_lc}}_relay_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    {{prefix_lc}}_relay_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    {{prefix_lc}}_relay_conf.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&{{prefix_lc}}_relay_conf));
    gpio_set_level((gpio_num_t){{prefix}}_RELAY_GPIO, !{{prefix}}_RELAY_ACTIVE_LEVEL);
    ESP_LOGI(TAG, "Relay {{prefix_lc}} initialized (GPIO %d, idle level %d)",
             {{prefix}}_RELAY_GPIO, !{{prefix}}_RELAY_ACTIVE_LEVEL);
}
