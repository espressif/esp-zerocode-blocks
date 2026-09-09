{
    esp_matter::endpoint::water_valve::config_t {{prefix_lc}}_cfg;
    /* esp_matter registers the delegate on the cluster object at server
     * startup (set_delegate_and_init_callback path). */
    {{prefix_lc}}_cfg.valve_configuration_and_control.delegate = &s_{{prefix_lc}}_valve_delegate;
    esp_matter::endpoint_t *ep = esp_matter::endpoint::water_valve::create(
        node, &{{prefix_lc}}_cfg, ENDPOINT_FLAG_NONE, NULL);
    if (!ep) { ESP_LOGE(TAG, "Failed to create {{prefix_lc}} endpoint"); return ESP_FAIL; }
    s_{{prefix_lc}}_endpoint_id = esp_matter::endpoint::get_id(ep);
    ESP_LOGI(TAG, "{{prefix_lc}}: water_valve endpoint id=%d", s_{{prefix_lc}}_endpoint_id);
}
