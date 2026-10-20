case {{cfg.locked_param}}: {
    /* The driver's own write-back of the sensed state: the bolt is already
       there, so driving it would only re-run the motor (or fight a jam). */
    if (s_{{prefix_lc}}_reporting && xTaskGetCurrentTaskHandle() == s_{{prefix_lc}}_report_task) {
        return ESP_OK;
    }
    if (!s_{{prefix_lc}}_pulse_timer) return ESP_ERR_INVALID_STATE;
    s_{{prefix_lc}}_cmd_locked = val.b;
    s_{{prefix_lc}}_driving = true;
    /* A new command during a pulse restarts the window toward the new end;
       stopping an idle timer returns INVALID_STATE, which is expected. */
    esp_timer_stop(s_{{prefix_lc}}_pulse_timer);
    if (val.b) {
        gpio_set_level((gpio_num_t){{prefix}}_UNLOCK_GPIO, 0);
        gpio_set_level((gpio_num_t){{prefix}}_LOCK_GPIO, 1);
    } else {
        gpio_set_level((gpio_num_t){{prefix}}_LOCK_GPIO, 0);
        gpio_set_level((gpio_num_t){{prefix}}_UNLOCK_GPIO, 1);
    }
    esp_timer_start_once(s_{{prefix_lc}}_pulse_timer, (uint64_t){{prefix}}_PULSE_MS * 1000ULL);
    ESP_LOGI(TAG, "Lock {{prefix_lc}}: driving to %s (%d ms)", val.b ? "LOCKED" : "UNLOCKED", {{prefix}}_PULSE_MS);
    return ESP_OK;
}
