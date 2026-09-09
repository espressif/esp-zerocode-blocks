case {{cfg.param_id}}: {
    int level = val.b ? {{prefix}}_RELAY_ACTIVE_LEVEL : !{{prefix}}_RELAY_ACTIVE_LEVEL;
    ESP_LOGI(TAG, "Set {{prefix_lc}} relay: %s", val.b ? "ON" : "OFF");
    return gpio_set_level((gpio_num_t){{prefix}}_RELAY_GPIO, level);
}
