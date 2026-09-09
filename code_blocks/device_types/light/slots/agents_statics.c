/* {{prefix_lc}}: local tools the cloud agent can invoke. Handlers run on the
 * agent component's task — they only touch the param bus (thread-safe) and
 * return a heap string (freed by the agent after the tool response). */
static esp_err_t {{prefix_lc}}_agents_set_power(esp_agent_handle_t handle, const char *tool_name,
                                                esp_agent_tool_param_t params[], size_t num_params,
                                                void *user_data, char **result)
{
    for (size_t i = 0; i < num_params; i++) {
        if (strcmp(params[i].name, "on") == 0 && params[i].type == ESP_AGENT_PARAM_TYPE_BOOL) {
            app_driver_param_val_t v = { .b = params[i].value.b };
            app_driver_set_param({{cfg.power_param}}, v, APP_DRIVER_SOURCE_LOCAL);
            *result = strdup(params[i].value.b ? "{{cfg.label}} turned on." : "{{cfg.label}} turned off.");
            return ESP_OK;
        }
    }
    *result = strdup("Missing bool parameter 'on'.");
    return ESP_ERR_INVALID_ARG;
}

static esp_err_t {{prefix_lc}}_agents_get_state(esp_agent_handle_t handle, const char *tool_name,
                                                esp_agent_tool_param_t params[], size_t num_params,
                                                void *user_data, char **result)
{
    app_driver_param_val_t v = {};
    app_driver_get_param({{cfg.power_param}}, &v);
    *result = strdup(v.b ? "{{cfg.label}} is on." : "{{cfg.label}} is off.");
    return ESP_OK;
}
