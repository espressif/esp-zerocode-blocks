{
    esp_matter::endpoint::dimmer_switch::config_t {{prefix_lc}}_cfg;
    esp_matter::endpoint_t *ep = esp_matter::endpoint::dimmer_switch::create(
        node, &{{prefix_lc}}_cfg, ENDPOINT_FLAG_NONE, NULL);
    if (!ep) { ESP_LOGE(TAG, "Failed to create {{prefix_lc}} endpoint"); return ESP_FAIL; }
    s_{{prefix_lc}}_endpoint_id = esp_matter::endpoint::get_id(ep);
    ESP_LOGI(TAG, "{{prefix_lc}}: dimmer_switch endpoint id=%d", s_{{prefix_lc}}_endpoint_id);
}
