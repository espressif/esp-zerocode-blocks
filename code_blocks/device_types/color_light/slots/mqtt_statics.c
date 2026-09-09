static char s_{{prefix_lc}}_state_topic[72] = {0};
static char s_{{prefix_lc}}_cmd_topic[72] = {0};

/* JSON-schema light: ONE payload carries state + brightness + hs color, so
 * every param change republishes the whole thing (retained — HA is right
 * after its own restart too). Driver hue/sat are Matter units (0-254);
 * HA hs is h 0-360 / s 0-100. BOTH directions round (+half a unit) — the
 * inbound conversion rounded and the outbound truncated, so HA sent h240/s80
 * and got back h239/s79 (measured on the emu round-trip). */
static void s_{{prefix_lc}}_publish_state(void)
{
    app_driver_param_val_t v = {};
    app_driver_get_param({{cfg.power_param}}, &v);
    bool on = v.b;
    app_driver_get_param({{cfg.brightness_param}}, &v);
    unsigned bri = v.u8;
    app_driver_get_param({{cfg.hue_param}}, &v);
    unsigned h = ((unsigned)v.u8 * 360u + 127u) / 254u;
    app_driver_get_param({{cfg.saturation_param}}, &v);
    unsigned s = ((unsigned)v.u8 * 100u + 127u) / 254u;
    char payload[160];
    snprintf(payload, sizeof(payload),
             "{\"state\":\"%s\",\"brightness\":%u,\"color_mode\":\"hs\",\"color\":{\"h\":%u,\"s\":%u}}",
             on ? "ON" : "OFF", bri, h > 360u ? 360u : h, s > 100u ? 100u : s);
    esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_state_topic, payload, 0, 1, true);
}
