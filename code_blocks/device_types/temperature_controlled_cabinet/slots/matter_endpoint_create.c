{
    esp_matter::endpoint::temperature_controlled_cabinet::config_t {{prefix_lc}}_cfg;
    /* TemperatureControl mandates EXACTLY ONE of TemperatureNumber/TemperatureLevel; default 0 aborts at boot. */
    {{prefix_lc}}_cfg.temperature_control.feature_flags |= esp_matter::cluster::temperature_control::feature::temperature_number::get_id();
    esp_matter::endpoint_t *ep = esp_matter::endpoint::temperature_controlled_cabinet::create(
        node, &{{prefix_lc}}_cfg, ENDPOINT_FLAG_NONE, NULL);
    if (!ep) { ESP_LOGE(TAG, "Failed to create {{prefix_lc}} endpoint"); return ESP_FAIL; }
    s_{{prefix_lc}}_endpoint_id = esp_matter::endpoint::get_id(ep);
    ESP_LOGI(TAG, "{{prefix_lc}}: temperature_controlled_cabinet endpoint id=%d", s_{{prefix_lc}}_endpoint_id);
}
