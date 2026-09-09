static esp_err_t {{prefix_lc}}_agents_get_temperature(esp_agent_handle_t handle, const char *tool_name,
                                                      esp_agent_tool_param_t params[], size_t num_params,
                                                      void *user_data, char **result)
{
    app_driver_param_val_t v = {};
    app_driver_get_param({{cfg.value_param}}, &v);
    /* i16 hundredths of °C, INT16_MIN = no reading yet — say so rather than
     * inventing a number (the agent relays this text to a person). */
    char buf[64];
    if (v.i16 == INT16_MIN) {
        snprintf(buf, sizeof(buf), "{{cfg.label}}: no reading available yet.");
    } else {
        snprintf(buf, sizeof(buf), "{{cfg.label}}: %.2f degrees C.", (double)v.i16 / 100.0);
    }
    *result = strdup(buf);
    return ESP_OK;
}
