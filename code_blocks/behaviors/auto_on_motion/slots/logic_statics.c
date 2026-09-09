static bool s_{{prefix_lc}}_motion_was = false;

static void {{prefix_lc}}_auto_on_cb(
    app_driver_param_id_t param_id, app_driver_param_val_t val,
    app_driver_handle_t source, void *ctx)
{
    if (param_id != {{cfg.motion_param}}) return;
    bool now = val.b;
    if (now && !s_{{prefix_lc}}_motion_was) {
        ESP_LOGI(TAG, "{{prefix_lc}}: motion detected — switching {{cfg.light_param}} on");
        app_driver_param_val_t pv = { .b = true };
        app_driver_set_param({{cfg.light_param}}, pv, APP_DRIVER_SOURCE_LOCAL);
    }
    s_{{prefix_lc}}_motion_was = now;
}
