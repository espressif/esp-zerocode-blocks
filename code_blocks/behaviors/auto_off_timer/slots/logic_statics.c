static esp_timer_handle_t s_{{prefix_lc}}_off_timer = NULL;

static void {{prefix_lc}}_off_arm(void)
{
    esp_timer_stop(s_{{prefix_lc}}_off_timer);
    ESP_ERROR_CHECK(esp_timer_start_once(s_{{prefix_lc}}_off_timer, (uint64_t){{prefix}}_AUTO_OFF_SECS * 1000000ULL));
}

static void {{prefix_lc}}_off_fire_cb(void *arg)
{
{{#if cfg.retrigger_param}}    /* Still occupied: a held-high sensor makes no new edge, so the expiry
       itself has to look. Re-arm instead of switching off. */
    app_driver_param_val_t {{prefix_lc}}_rt = {};
    app_driver_get_param({{cfg.retrigger_param}}, &{{prefix_lc}}_rt);
    if ({{prefix_lc}}_rt.b) {
        {{prefix_lc}}_off_arm();
        ESP_LOGI(TAG, "{{prefix_lc}}: auto-off postponed — {{cfg.retrigger_param}} still active");
        return;
    }
{{/if}}    ESP_LOGI(TAG, "{{prefix_lc}}: auto-off timer expired — clearing {{cfg.target_param}}");
    app_driver_param_val_t pv = { .b = false };
    app_driver_set_param({{cfg.target_param}}, pv, APP_DRIVER_SOURCE_LOCAL);
}

static void {{prefix_lc}}_auto_off_cb(
    app_driver_param_id_t param_id, app_driver_param_val_t val,
    app_driver_handle_t source, void *ctx)
{
    if (s_{{prefix_lc}}_off_timer == NULL) return;
{{#if cfg.retrigger_param}}    if (param_id == {{cfg.retrigger_param}}) {
        /* Any motion change while the target is on restarts the countdown. */
        app_driver_param_val_t {{prefix_lc}}_on = {};
        app_driver_get_param({{cfg.target_param}}, &{{prefix_lc}}_on);
        if ({{prefix_lc}}_on.b) {
            {{prefix_lc}}_off_arm();
            ESP_LOGI(TAG, "{{prefix_lc}}: {{cfg.retrigger_param}} changed — auto-off restarted (%u s)", {{prefix}}_AUTO_OFF_SECS);
        }
        return;
    }
{{/if}}    if (param_id != {{cfg.target_param}}) return;
    if (val.b) {
        {{prefix_lc}}_off_arm();
        ESP_LOGI(TAG, "{{prefix_lc}}: auto-off armed for %u s", {{prefix}}_AUTO_OFF_SECS);
    } else {
        esp_timer_stop(s_{{prefix_lc}}_off_timer);
    }
}
