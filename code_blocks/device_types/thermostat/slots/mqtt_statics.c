static char s_{{prefix_lc}}_curr_topic[72] = {0};
static char s_{{prefix_lc}}_temp_state_topic[72] = {0};
static char s_{{prefix_lc}}_temp_cmd_topic[72] = {0};
static char s_{{prefix_lc}}_mode_state_topic[72] = {0};
static char s_{{prefix_lc}}_mode_cmd_topic[72] = {0};
{{#if cfg.heating_param}}
static char s_{{prefix_lc}}_action_topic[72] = {0};

/* HA climate action: "heating" while the heat output is on — whatever the
 * mode says, it is what the heater is doing — else "off" in mode Off, else
 * "idle". Nothing until the heat output has been set at least once, and only
 * "heating" until the mode has: HA shows unknown rather than a guess. */
static void {{prefix_lc}}_publish_action(void)
{
    if (!app_driver_param_seen({{cfg.heating_param}})) return;
    app_driver_param_val_t h = {}, m = {};
    app_driver_get_param({{cfg.heating_param}}, &h);
    if (!h.b && !app_driver_param_seen({{cfg.system_mode_param}})) return;
    app_driver_get_param({{cfg.system_mode_param}}, &m);
    zc_mqtt_publish(s_{{prefix_lc}}_action_topic, h.b ? "heating" : (m.u8 == 0 ? "off" : "idle"), 0, 1, true);
}
{{/if}}
