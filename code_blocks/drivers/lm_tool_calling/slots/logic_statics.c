/* ── {{prefix}}: language commands for {{cfg.device}} / {{cfg.location}} ── */

/* Runs on app_lm's inference worker (core 1), not on a caller's task. Keep it
 * short: it is holding the only decode slot. */
static void {{prefix_lc}}_on_action(const char *action, int value, bool has_value, void *arg)
{
    (void)arg;
    (void)value;        /* used only when this instance wires a value_param */

    if (strcmp(action, "on") == 0 || strcmp(action, "off") == 0) {
        app_driver_param_val_t v = { .b = (strcmp(action, "on") == 0) };
        app_driver_set_param({{cfg.power_param}}, v, APP_DRIVER_SOURCE_LOCAL);
        return;
    }
{{#if cfg.value_param}}

    if (strcmp(action, "set") == 0 && has_value) {
        /* The schema's value is already in the device's own units — 22 for an
         * AC is 22 degrees, not a percentage. Clamped only to the param width. */
        if (value < 0)   { value = 0; }
        if (value > 255) { value = 255; }
        app_driver_param_val_t sv = { .u8 = (uint8_t)value };
        app_driver_set_param({{cfg.value_param}}, sv, APP_DRIVER_SOURCE_LOCAL);
    }
{{/if}}
    (void)has_value;
}
