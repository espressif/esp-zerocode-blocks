{
    const esp_timer_create_args_t {{prefix_lc}}_off_args = {
        .callback = &{{prefix_lc}}_off_fire_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "{{prefix_lc}}_off",
        .skip_unhandled_events = true,
    };
    ESP_ERROR_CHECK(esp_timer_create(&{{prefix_lc}}_off_args, &s_{{prefix_lc}}_off_timer));
    app_driver_register_solution("{{prefix_lc}}_auto_off", {{prefix_lc}}_auto_off_cb, NULL);
    /* The timer arms on a notification, and a target already on before we
     * registered (nvs_persist_bool restored it, a framework did) sent none —
     * that load stayed on with no auto-off after every power cut. */
    app_driver_param_val_t {{prefix_lc}}_init_on = {};
    if (app_driver_param_seen({{cfg.target_param}}) &&
        app_driver_get_param({{cfg.target_param}}, &{{prefix_lc}}_init_on) == ESP_OK &&
        {{prefix_lc}}_init_on.b) {
        {{prefix_lc}}_off_arm();
        ESP_LOGI(TAG, "{{prefix_lc}}: {{cfg.target_param}} already on at boot — auto-off armed for %u s", {{prefix}}_AUTO_OFF_SECS);
    }
}
