case {{cfg.param_id}}: {
    ESP_LOGI(TAG, "Set {{prefix_lc}} ('{{cfg.device}}'): %s", val.b ? "ON" : "OFF");
    {{prefix_lc}}_board_gpio_set(val.b);
    return ESP_OK;
}
