static esp_timer_handle_t s_{{prefix_lc}}_off_timer = NULL;

static void {{prefix_lc}}_off_fire_cb(void *arg)
{
    ESP_LOGI(TAG, "{{prefix_lc}}: auto-off timer expired — clearing {{cfg.target_param}}");
    app_driver_param_val_t pv = { .b = false };
    app_driver_set_param({{cfg.target_param}}, pv, APP_DRIVER_SOURCE_LOCAL);
}

static void {{prefix_lc}}_auto_off_cb(
    app_driver_param_id_t param_id, app_driver_param_val_t val,
    app_driver_handle_t source, void *ctx)
{
    if (param_id != {{cfg.target_param}}) return;
    if (s_{{prefix_lc}}_off_timer == NULL) return;
    if (val.b) {
        esp_timer_stop(s_{{prefix_lc}}_off_timer);
        ESP_ERROR_CHECK(esp_timer_start_once(s_{{prefix_lc}}_off_timer, (uint64_t){{prefix}}_AUTO_OFF_SECS * 1000000ULL));
        ESP_LOGI(TAG, "{{prefix_lc}}: auto-off armed for %u s", {{prefix}}_AUTO_OFF_SECS);
    } else {
        esp_timer_stop(s_{{prefix_lc}}_off_timer);
    }
}
