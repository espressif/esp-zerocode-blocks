{
    /* Idle line is high (external pull-up). Configure as input with the
     * internal pull-up as a fallback; an external 4.7k-10k is recommended. */
    gpio_config_t {{prefix_lc}}_io = {
        .pin_bit_mask = 1ULL << {{prefix}}_DHT_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&{{prefix_lc}}_io);

    /* Clamp poll interval to the DHT max sample rate (~0.5 Hz). */
    uint64_t {{prefix_lc}}_period_ms = (uint64_t){{prefix}}_DHT_POLL_MS;
    if ({{prefix_lc}}_period_ms < 2000) {{prefix_lc}}_period_ms = 2000;

    const esp_timer_create_args_t {{prefix_lc}}_args = {
        .callback = &{{prefix_lc}}_dht_poll_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "{{prefix_lc}}_dht",
        .skip_unhandled_events = true,
    };
    esp_timer_handle_t {{prefix_lc}}_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&{{prefix_lc}}_args, &{{prefix_lc}}_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic({{prefix_lc}}_timer, {{prefix_lc}}_period_ms * 1000ULL));
    ESP_LOGI(TAG, "DHT {{prefix_lc}}: polling GPIO %d every %llu ms (dht11=%d)",
             {{prefix}}_DHT_GPIO, {{prefix_lc}}_period_ms, {{prefix}}_DHT_IS_DHT11);
}
