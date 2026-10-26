{
    gpio_config_t {{prefix_lc}}_vib_cfg = {};
    {{prefix_lc}}_vib_cfg.pin_bit_mask = (1ULL << {{prefix}}_VIB_GPIO);
    {{prefix_lc}}_vib_cfg.mode = GPIO_MODE_INPUT;
#if defined({{prefix}}_VIB_PULL_pullup)
    {{prefix_lc}}_vib_cfg.pull_up_en = GPIO_PULLUP_ENABLE;
#elif defined({{prefix}}_VIB_PULL_pulldown)
    {{prefix_lc}}_vib_cfg.pull_down_en = GPIO_PULLDOWN_ENABLE;
#endif
    /* Count edges INTO the active level: one per pulse. */
    {{prefix_lc}}_vib_cfg.intr_type = {{prefix}}_VIB_ACTIVE_LEVEL ? GPIO_INTR_POSEDGE : GPIO_INTR_NEGEDGE;
    ESP_ERROR_CHECK(gpio_config(&{{prefix_lc}}_vib_cfg));

    /* INVALID_STATE = another block installed the shared service already. */
    esp_err_t {{prefix_lc}}_vib_err = gpio_install_isr_service(0);
    if ({{prefix_lc}}_vib_err == ESP_ERR_INVALID_STATE) {{prefix_lc}}_vib_err = ESP_OK;
    if ({{prefix_lc}}_vib_err == ESP_OK) {
        {{prefix_lc}}_vib_err = gpio_isr_handler_add((gpio_num_t){{prefix}}_VIB_GPIO, {{prefix_lc}}_vib_isr, NULL);
    }
    if ({{prefix_lc}}_vib_err != ESP_OK) {
        /* The window still runs and reports 0 — a still machine, not a crash. */
        ESP_LOGE(TAG, "Vibration switch {{prefix_lc}}: interrupt on GPIO %d not attached (%s) — activity stays 0",
                 {{prefix}}_VIB_GPIO, esp_err_to_name({{prefix_lc}}_vib_err));
    }

    const esp_timer_create_args_t {{prefix_lc}}_vib_args = {
        .callback = &{{prefix_lc}}_vib_window_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "{{prefix_lc}}_vib",
        .skip_unhandled_events = true,
    };
    esp_timer_handle_t {{prefix_lc}}_vib_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&{{prefix_lc}}_vib_args, &{{prefix_lc}}_vib_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic({{prefix_lc}}_vib_timer, (uint64_t){{prefix}}_VIB_WINDOW_MS * 1000ULL));
{{#if cfg.stale_param}}
    /* Healthy until stale_after_s passes with no pulse. */
    app_driver_param_val_t {{prefix_lc}}_vib_fresh = { .b = false };
    app_driver_set_param({{cfg.stale_param}}, {{prefix_lc}}_vib_fresh, APP_DRIVER_SOURCE_LOCAL);
    if ({{prefix}}_VIB_STALE_MS == 0) {
        ESP_LOGW(TAG, "Vibration switch {{prefix_lc}}: stale_param is bound but stale_after_s is 0 — it never turns true");
    }
{{/if}}
    ESP_LOGI(TAG, "Vibration switch {{prefix_lc}}: GPIO %d, active=%d, %d ms window",
             {{prefix}}_VIB_GPIO, {{prefix}}_VIB_ACTIVE_LEVEL, {{prefix}}_VIB_WINDOW_MS);
}
