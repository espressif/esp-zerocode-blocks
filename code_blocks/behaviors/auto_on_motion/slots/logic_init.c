{
    app_driver_register_solution("{{prefix_lc}}_auto_on", {{prefix_lc}}_auto_on_cb, NULL);
    /* Motion already reported before this registered (a sensor that read
     * "occupied" at boot) sent its edge to nobody: catch up now, as
     * auto_off_timer does for a load already on. */
    app_driver_param_val_t {{prefix_lc}}_m = {};
    if (app_driver_param_seen({{cfg.motion_param}}) &&
        app_driver_get_param({{cfg.motion_param}}, &{{prefix_lc}}_m) == ESP_OK && {{prefix_lc}}_m.b &&
        !s_{{prefix_lc}}_motion_was) {
        s_{{prefix_lc}}_motion_was = true;
        ESP_LOGI(TAG, "{{prefix_lc}}: motion already present at boot — switching {{cfg.light_param}} on");
        app_driver_param_val_t {{prefix_lc}}_on = { .b = true };
        app_driver_set_param({{cfg.light_param}}, {{prefix_lc}}_on, APP_DRIVER_SOURCE_LOCAL);
    }
}
