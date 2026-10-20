{
    /* Idle line is high (external pull-up). Configure as input with the
     * internal pull-up as a fallback; an external 4.7k-10k is recommended. */
    gpio_config_t {{prefix_lc}}_io = {};
    {{prefix_lc}}_io.pin_bit_mask = 1ULL << {{prefix}}_DHT_GPIO;
    {{prefix_lc}}_io.mode = GPIO_MODE_INPUT;
    {{prefix_lc}}_io.pull_up_en = GPIO_PULLUP_ENABLE;
    {{prefix_lc}}_io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    {{prefix_lc}}_io.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&{{prefix_lc}}_io);

    /* Polled from its own task (see logic_statics). The period is clamped
       to the DHT max sample rate (~0.5 Hz) in config_defines. */
    xTaskCreate({{prefix_lc}}_dht_task, "{{prefix_lc}}_dht", 3072, NULL, 3, NULL);
    ESP_LOGI(TAG, "DHT {{prefix_lc}}: polling GPIO %d every %lu ms (dht11=%d)",
             {{prefix}}_DHT_GPIO, (unsigned long){{prefix}}_DHT_PERIOD_MS, {{prefix}}_DHT_IS_DHT11);
}
