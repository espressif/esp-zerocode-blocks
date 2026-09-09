case {{cfg.locked_param}}: {
    if (val.b) {
        gpio_set_level((gpio_num_t){{prefix}}_UNLOCK_GPIO, 0);
        gpio_set_level((gpio_num_t){{prefix}}_LOCK_GPIO, 1);
    } else {
        gpio_set_level((gpio_num_t){{prefix}}_LOCK_GPIO, 0);
        gpio_set_level((gpio_num_t){{prefix}}_UNLOCK_GPIO, 1);
    }
    vTaskDelay(pdMS_TO_TICKS({{prefix}}_PULSE_MS));
    gpio_set_level((gpio_num_t){{prefix}}_LOCK_GPIO, 0);
    gpio_set_level((gpio_num_t){{prefix}}_UNLOCK_GPIO, 0);
    ESP_LOGI(TAG, "Lock {{prefix_lc}}: %s", val.b ? "LOCKED" : "UNLOCKED");
    return ESP_OK;
}
