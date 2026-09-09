static bool s_{{prefix_lc}}_asserted = false;

static void {{prefix_lc}}_alarm_poll_cb(void *arg)
{
    int level = gpio_get_level((gpio_num_t){{prefix}}_ALARM_GPIO);
    bool asserted = (level == {{prefix}}_ALARM_ACTIVE_LEVEL);
    if (asserted != s_{{prefix_lc}}_asserted) {
        s_{{prefix_lc}}_asserted = asserted;
        /* Explicit cast: the ternary is int, target is uint8_t (-Werror=narrowing). */
        uint8_t state = (uint8_t)(asserted ? {{prefix}}_ALARM_ASSERTED : {{prefix}}_ALARM_CLEARED);
        app_driver_param_val_t pv = { .u8 = state };
        app_driver_set_param({{cfg.target_param}}, pv, APP_DRIVER_SOURCE_LOCAL);
        ESP_LOGI(TAG, "{{prefix_lc}}: %s (state=%u)", asserted ? "ALARM" : "cleared", (unsigned)state);
    }
}
