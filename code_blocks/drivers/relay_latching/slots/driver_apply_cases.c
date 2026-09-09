case {{cfg.param_id}}: {
    gpio_num_t coil = (gpio_num_t)(val.b ? {{prefix}}_LATCH_SET_GPIO : {{prefix}}_LATCH_RESET_GPIO);
    gpio_set_level(coil, {{prefix}}_LATCH_LEVEL);
    vTaskDelay(pdMS_TO_TICKS({{prefix}}_LATCH_PULSE_MS));
    gpio_set_level(coil, !{{prefix}}_LATCH_LEVEL);
    ESP_LOGI(TAG, "Latching relay {{prefix_lc}}: %s", val.b ? "ON" : "OFF");
    return ESP_OK;
}
