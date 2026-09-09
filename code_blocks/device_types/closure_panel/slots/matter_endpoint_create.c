{
    esp_matter::endpoint::closure_panel::config_t {{prefix_lc}}_cfg;
    /* ClosureDimension needs one of Positioning/MotionLatching; and esp_matter's
     * create() requires EXACTLY ONE of Translation/Rotation/Modulation whenever
     * Positioning is set (else the cluster create aborts at boot). A shutter
     * panel moves linearly, so pair Positioning with Translation. */
    {{prefix_lc}}_cfg.closure_dimension.delegate = &s_{{prefix_lc}}_panel_delegate;
    {{prefix_lc}}_cfg.closure_dimension.feature_flags =
        esp_matter::cluster::closure_dimension::feature::positioning::get_id() |
        esp_matter::cluster::closure_dimension::feature::translation::get_id();
    esp_matter::endpoint_t *ep = esp_matter::endpoint::closure_panel::create(
        node, &{{prefix_lc}}_cfg, ENDPOINT_FLAG_NONE, NULL);
    if (!ep) { ESP_LOGE(TAG, "Failed to create {{prefix_lc}} endpoint"); return ESP_FAIL; }
    s_{{prefix_lc}}_endpoint_id = esp_matter::endpoint::get_id(ep);
    ESP_LOGI(TAG, "{{prefix_lc}}: closure_panel endpoint id=%d", s_{{prefix_lc}}_endpoint_id);
}
