{
    esp_matter::endpoint::water_heater::config_t {{prefix_lc}}_cfg;
    /* Thermostat cluster mandates >=1 of Heat/Cool; default feature_flags=0 aborts at boot. */
    {{prefix_lc}}_cfg.thermostat.feature_flags |= esp_matter::cluster::thermostat::feature::heating::get_id();
    esp_matter::endpoint_t *ep = esp_matter::endpoint::water_heater::create(
        node, &{{prefix_lc}}_cfg, ENDPOINT_FLAG_NONE, NULL);
    if (!ep) { ESP_LOGE(TAG, "Failed to create {{prefix_lc}} endpoint"); return ESP_FAIL; }
    s_{{prefix_lc}}_endpoint_id = esp_matter::endpoint::get_id(ep);
    ESP_LOGI(TAG, "{{prefix_lc}}: water_heater endpoint id=%d", s_{{prefix_lc}}_endpoint_id);
}
