{
    esp_matter::endpoint::occupancy_sensor::config_t {{prefix_lc}}_cfg;
    /* OccupancySensing mandates >=1 sensor-type feature; default feature_flags=0 aborts at boot. */
    {{prefix_lc}}_cfg.occupancy_sensing.feature_flags |= esp_matter::cluster::occupancy_sensing::feature::passive_infrared::get_id();
    esp_matter::endpoint_t *ep = esp_matter::endpoint::occupancy_sensor::create(
        node, &{{prefix_lc}}_cfg, ENDPOINT_FLAG_NONE, NULL);
    if (!ep) { ESP_LOGE(TAG, "Failed to create {{prefix_lc}} endpoint"); return ESP_FAIL; }
    s_{{prefix_lc}}_endpoint_id = esp_matter::endpoint::get_id(ep);
    ESP_LOGI(TAG, "{{prefix_lc}}: occupancy_sensor endpoint id=%d", s_{{prefix_lc}}_endpoint_id);
}
