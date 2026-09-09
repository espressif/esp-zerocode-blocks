{
    esp_matter::endpoint::closure::config_t {{prefix_lc}}_cfg;
    /* ClosureControl mandates one of Positioning/MotionLatching; default feature_flags=0 aborts at boot. */
    {{prefix_lc}}_cfg.closure_control.delegate = &s_{{prefix_lc}}_closure_delegate;
    {{prefix_lc}}_cfg.closure_control.feature_flags = esp_matter::cluster::closure_control::feature::positioning::get_id();
    esp_matter::endpoint_t *ep = esp_matter::endpoint::closure::create(
        node, &{{prefix_lc}}_cfg, ENDPOINT_FLAG_NONE, NULL);
    if (!ep) { ESP_LOGE(TAG, "Failed to create {{prefix_lc}} endpoint"); return ESP_FAIL; }
    s_{{prefix_lc}}_endpoint_id = esp_matter::endpoint::get_id(ep);
    ESP_LOGI(TAG, "{{prefix_lc}}: closure endpoint id=%d", s_{{prefix_lc}}_endpoint_id);
}
