{
    onewire_bus_config_t {{prefix_lc}}_bus_cfg = { .bus_gpio_num = {{prefix}}_OW_GPIO };
    onewire_bus_rmt_config_t {{prefix_lc}}_rmt_cfg = { .max_rx_bytes = 16 };
    /* Degraded, not fatal: RMT channels are a small pool shared with LED
       strips and IR, and running out must not boot-loop the product. The
       handle stays NULL, which every 1-Wire consumer checks. */
    esp_err_t {{prefix_lc}}_err = onewire_new_bus_rmt(&{{prefix_lc}}_bus_cfg, &{{prefix_lc}}_rmt_cfg, &s_{{prefix_lc}}_onewire);
    if ({{prefix_lc}}_err != ESP_OK) {
        s_{{prefix_lc}}_onewire = NULL;
        ESP_LOGE(TAG, "1-Wire bus {{prefix_lc}}: GPIO=%d not started (%s) — no free RMT channel?", {{prefix}}_OW_GPIO, esp_err_to_name({{prefix_lc}}_err));
    } else {
        ESP_LOGI(TAG, "1-Wire bus {{prefix_lc}}: GPIO=%d", {{prefix}}_OW_GPIO);
    }
}
