{
    /* Idle level BEFORE the mode switch — gpio_config() drives immediately and
     * the output register resets to 0, so an active-LOW relay was energized for
     * the gap between config and deassert. See drivers/relay. */
    gpio_set_level((gpio_num_t){{prefix}}_RELAY_GPIO, !{{prefix}}_RELAY_ACTIVE_LEVEL);
    gpio_config_t {{prefix_lc}}_relay_conf = {
        .pin_bit_mask = (1ULL << {{prefix}}_RELAY_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&{{prefix_lc}}_relay_conf));
    gpio_set_level((gpio_num_t){{prefix}}_RELAY_GPIO, !{{prefix}}_RELAY_ACTIVE_LEVEL);
    ESP_LOGI(TAG, "Relay+button {{prefix_lc}}: relay GPIO %d, button GPIO %d", {{prefix}}_RELAY_GPIO, {{prefix}}_BUTTON_GPIO);
}
