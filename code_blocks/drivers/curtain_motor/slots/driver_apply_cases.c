case {{cfg.position_param}}: {
    /* Bang-bang: drive toward the end the target is closer to; the limit poll
       in driver_statics stops the motor and reports the end reached. */
    int dir = (val.u16 > 5000) ? 1 : -1;
    bool already = false;
    portENTER_CRITICAL(&s_{{prefix_lc}}_mux);
    if ({{prefix_lc}}_at_limit(dir)) {
        already = true;
        {{prefix_lc}}_motor_off();
        s_{{prefix_lc}}_dir = 0;
    } else if (dir != s_{{prefix_lc}}_dir) {
        /* Opposite leg low FIRST so the H-bridge never sees both high. A repeat
           of the current direction keeps its start time, so re-sending a
           target cannot extend a run past max_run_ms. */
        if (dir > 0) {
            gpio_set_level((gpio_num_t){{prefix}}_DOWN_GPIO, 0);
            gpio_set_level((gpio_num_t){{prefix}}_UP_GPIO, 1);
        } else {
            gpio_set_level((gpio_num_t){{prefix}}_UP_GPIO, 0);
            gpio_set_level((gpio_num_t){{prefix}}_DOWN_GPIO, 1);
        }
        s_{{prefix_lc}}_dir = dir;
        s_{{prefix_lc}}_started_us = esp_timer_get_time();
    }
    portEXIT_CRITICAL(&s_{{prefix_lc}}_mux);
    /* Moving: between the ends until a limit says where it stopped. */
    if (!already) {{prefix_lc}}_report_current(0xFFFF);
    else {{prefix_lc}}_report_current((uint16_t)(dir > 0 ? 10000 : 0));
    ESP_LOGI(TAG, "Curtain {{prefix_lc}}: target=%u%% → %s%s", val.u16 / 100U,
             dir > 0 ? "open" : "close", already ? " (already at limit)" : "");
    return ESP_OK;
}
{{#if cfg.stop_param}}case {{cfg.stop_param}}: {
    int was;
    portENTER_CRITICAL(&s_{{prefix_lc}}_mux);
    was = s_{{prefix_lc}}_dir;
    {{prefix_lc}}_motor_off();
    s_{{prefix_lc}}_dir = 0;
    portEXIT_CRITICAL(&s_{{prefix_lc}}_mux);
    if (was != 0) {
        /* Stopped between the ends: the position is not known. */
        {{prefix_lc}}_report_current(0xFFFF);
        ESP_LOGI(TAG, "Curtain {{prefix_lc}}: stopped");
    }
    return ESP_OK;
}{{/if}}
