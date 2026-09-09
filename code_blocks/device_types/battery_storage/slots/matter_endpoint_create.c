{
    esp_matter::endpoint::battery_storage::config_t {{prefix_lc}}_cfg;
    /* ElectricalEnergyMeasurement mandates >=1 of Imported/Exported AND >=1 of Cumulative/Periodic;
     * default feature_flags=0 aborts at boot. */
    {{prefix_lc}}_cfg.electrical_energy_measurement.feature_flags |=
        esp_matter::cluster::electrical_energy_measurement::feature::imported_energy::get_id() |
        esp_matter::cluster::electrical_energy_measurement::feature::cumulative_energy::get_id();
    esp_matter::endpoint_t *ep = esp_matter::endpoint::battery_storage::create(
        node, &{{prefix_lc}}_cfg, ENDPOINT_FLAG_NONE, NULL);
    if (!ep) { ESP_LOGE(TAG, "Failed to create {{prefix_lc}} endpoint"); return ESP_FAIL; }
    s_{{prefix_lc}}_endpoint_id = esp_matter::endpoint::get_id(ep);
    ESP_LOGI(TAG, "{{prefix_lc}}: battery_storage endpoint id=%d", s_{{prefix_lc}}_endpoint_id);
}
