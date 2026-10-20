{
    esp_matter::endpoint::thermostat::config_t {{prefix_lc}}_cfg;
    /* Thermostat cluster mandates >=1 of Heat/Cool; default feature_flags=0 aborts at boot. */
    {{prefix_lc}}_cfg.thermostat.feature_flags |= esp_matter::cluster::thermostat::feature::heating::get_id();
    /* ControlSequenceOfOperation defaults to 0 = CoolingOnly in the generated
     * data model, and CHIP then refuses SystemMode=Heat (InvalidValue) — from a
     * controller AND from this block's own driver push. A heat-only thermostat
     * is 2 = HeatingOnly. SystemMode starts from the driver: Off or Heat. */
    {{prefix_lc}}_cfg.thermostat.control_sequence_of_operation = 2;
    {
        app_driver_param_val_t init = {};
        app_driver_get_param({{cfg.system_mode_param}}, &init);
        {{prefix_lc}}_cfg.thermostat.system_mode = (init.u8 == 0) ? 0 : 4;
    }
    esp_matter::endpoint_t *ep = esp_matter::endpoint::thermostat::create(
        node, &{{prefix_lc}}_cfg, ENDPOINT_FLAG_NONE, NULL);
    if (!ep) { ESP_LOGE(TAG, "Failed to create {{prefix_lc}} endpoint"); return ESP_FAIL; }
    s_{{prefix_lc}}_endpoint_id = esp_matter::endpoint::get_id(ep);
    ESP_LOGI(TAG, "{{prefix_lc}}: thermostat endpoint id=%d", s_{{prefix_lc}}_endpoint_id);
}
