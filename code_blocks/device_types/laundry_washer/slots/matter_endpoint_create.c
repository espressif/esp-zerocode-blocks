{
    esp_matter::endpoint::laundry_washer::config_t {{prefix_lc}}_cfg;
    {{prefix_lc}}_cfg.operational_state.delegate = &s_{{prefix_lc}}_opstate;
    esp_matter::endpoint_t *ep = esp_matter::endpoint::laundry_washer::create(
        node, &{{prefix_lc}}_cfg, ENDPOINT_FLAG_NONE, NULL);
    if (!ep) { ESP_LOGE(TAG, "Failed to create {{prefix_lc}} endpoint"); return ESP_FAIL; }
    s_{{prefix_lc}}_endpoint_id = esp_matter::endpoint::get_id(ep);
    ESP_LOGI(TAG, "{{prefix_lc}}: laundry_washer endpoint id=%d", s_{{prefix_lc}}_endpoint_id);
    {{#if cfg.mode_param}}
    {
        esp_matter::cluster::laundry_washer_mode::config_t {{prefix_lc}}_mode_cfg;
        {{prefix_lc}}_mode_cfg.delegate = &s_{{prefix_lc}}_mode;
        esp_matter::cluster::laundry_washer_mode::create(ep, &{{prefix_lc}}_mode_cfg, CLUSTER_FLAG_SERVER);
    }
    {{/if}}
}
