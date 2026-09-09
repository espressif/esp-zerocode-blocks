{
    onewire_bus_config_t {{prefix_lc}}_bus_cfg = { .bus_gpio_num = {{prefix}}_OW_GPIO };
    onewire_bus_rmt_config_t {{prefix_lc}}_rmt_cfg = { .max_rx_bytes = 16 };
    ESP_ERROR_CHECK(onewire_new_bus_rmt(&{{prefix_lc}}_bus_cfg, &{{prefix_lc}}_rmt_cfg, &s_{{prefix_lc}}_onewire));
    ESP_LOGI(TAG, "1-Wire bus {{prefix_lc}}: GPIO=%d", {{prefix}}_OW_GPIO);
}
