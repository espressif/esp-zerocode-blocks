{
    /* Idle level BEFORE the mode switch on BOTH coils — gpio_config() drives
     * immediately and the output register resets to 0, so an active-LOW coil
     * would get a pulse wide enough to actually latch. See drivers/relay. */
    gpio_set_level((gpio_num_t){{prefix}}_LATCH_SET_GPIO, !{{prefix}}_LATCH_LEVEL);
    gpio_set_level((gpio_num_t){{prefix}}_LATCH_RESET_GPIO, !{{prefix}}_LATCH_LEVEL);
    gpio_config_t {{prefix_lc}}_latch_cfg = {
        .pin_bit_mask = (1ULL << {{prefix}}_LATCH_SET_GPIO) | (1ULL << {{prefix}}_LATCH_RESET_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&{{prefix_lc}}_latch_cfg));
    gpio_set_level((gpio_num_t){{prefix}}_LATCH_SET_GPIO, !{{prefix}}_LATCH_LEVEL);
    gpio_set_level((gpio_num_t){{prefix}}_LATCH_RESET_GPIO, !{{prefix}}_LATCH_LEVEL);
    ESP_LOGI(TAG, "Latching relay {{prefix_lc}}: set=%d reset=%d", {{prefix}}_LATCH_SET_GPIO, {{prefix}}_LATCH_RESET_GPIO);
}
