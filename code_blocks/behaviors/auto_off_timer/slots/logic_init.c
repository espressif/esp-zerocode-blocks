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
}
