static button_handle_t s_{{prefix_lc}}_button = NULL;

/* Optional override — implement this in your app code to react to {{prefix_lc}} press.
 * Default no-op so the linker is happy when no custom handler is provided. */
__attribute__((weak)) void {{prefix_lc}}_on_press(void)
{
    ESP_LOGI(TAG, "{{prefix_lc}}: button pressed (no handler installed)");
}

static void {{prefix_lc}}_button_cb(void *arg, void *usr_data)
{
{{#if cfg.position_param}}
    /* A click is an EVENT: fire_event, so a second click is not swallowed as
     * "unchanged". Every scene_button binding reads one notification as one press. */
    app_driver_param_val_t v = { .u8 = 1 };
    app_driver_fire_event({{cfg.position_param}}, v, APP_DRIVER_SOURCE_LOCAL);
{{/if}}
    {{prefix_lc}}_on_press();
}
