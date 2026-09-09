{
    esp_matter::endpoint::generic_switch::config_t {{prefix_lc}}_cfg;
    /* Switch cluster mandates EXACTLY ONE of Latching/Momentary; default feature_flags=0 aborts at boot. */
    {{prefix_lc}}_cfg.switch_cluster.feature_flags |= esp_matter::cluster::switch_cluster::feature::momentary_switch::get_id();
    esp_matter::endpoint_t *ep = esp_matter::endpoint::generic_switch::create(
        node, &{{prefix_lc}}_cfg, ENDPOINT_FLAG_NONE, NULL);
    if (!ep) { ESP_LOGE(TAG, "Failed to create {{prefix_lc}} endpoint"); return ESP_FAIL; }
    s_{{prefix_lc}}_endpoint_id = esp_matter::endpoint::get_id(ep);
    ESP_LOGI(TAG, "{{prefix_lc}}: generic_switch endpoint id=%d", s_{{prefix_lc}}_endpoint_id);
}
