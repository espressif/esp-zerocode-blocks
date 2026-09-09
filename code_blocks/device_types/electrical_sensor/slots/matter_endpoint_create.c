{
    esp_matter::endpoint::electrical_sensor::config_t {{prefix_lc}}_cfg;
    /* electrical_sensor creates two mandatory-feature clusters whose default feature_flags=0 abort at boot:
     *   ElectricalPowerMeasurement (>=1 of DC/AC) and PowerTopology (EXACTLY ONE topology). */
    {{prefix_lc}}_cfg.electrical_power_measurement.feature_flags |= esp_matter::cluster::electrical_power_measurement::feature::alternating_current::get_id();
    {{prefix_lc}}_cfg.power_topology.feature_flags |= esp_matter::cluster::power_topology::feature::node_topology::get_id();
    /* Pass the Delegate so ElectricalPowerMeasurementDelegateInitCB creates the
     * server Instance (registers the AttributeAccessInterface that supplies
     * ActivePower). Without it, ActivePower is MANAGED_INTERNALLY with no read
     * handler and the endpoint has no way to serve the value. */
    {{prefix_lc}}_cfg.electrical_power_measurement.delegate = &s_{{prefix_lc}}_epm_delegate;
    esp_matter::endpoint_t *ep = esp_matter::endpoint::electrical_sensor::create(
        node, &{{prefix_lc}}_cfg, ENDPOINT_FLAG_NONE, NULL);
    if (!ep) { ESP_LOGE(TAG, "Failed to create {{prefix_lc}} endpoint"); return ESP_FAIL; }
    s_{{prefix_lc}}_endpoint_id = esp_matter::endpoint::get_id(ep);
    ESP_LOGI(TAG, "{{prefix_lc}}: electrical_sensor endpoint id=%d", s_{{prefix_lc}}_endpoint_id);
}
