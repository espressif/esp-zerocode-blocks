{
    esp_matter::endpoint::pump::config_t {{prefix_lc}}_cfg;
    /* PumpConfigAndControl mandates >=1 operation feature; default feature_flags=0 aborts at boot. */
    {{prefix_lc}}_cfg.pump_configuration_and_control.feature_flags |= esp_matter::cluster::pump_configuration_and_control::feature::constant_speed::get_id();
    esp_matter::endpoint_t *ep = esp_matter::endpoint::pump::create(
        node, &{{prefix_lc}}_cfg, ENDPOINT_FLAG_NONE, NULL);
    if (!ep) { ESP_LOGE(TAG, "Failed to create {{prefix_lc}} endpoint"); return ESP_FAIL; }
    s_{{prefix_lc}}_endpoint_id = esp_matter::endpoint::get_id(ep);
    ESP_LOGI(TAG, "{{prefix_lc}}: pump endpoint id=%d", s_{{prefix_lc}}_endpoint_id);
}
