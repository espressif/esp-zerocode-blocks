{
    esp_matter::endpoint::electrical_sensor::config_t {{prefix_lc}}_cfg;
    /* esp_matter 1.6: ElectricalPowerMeasurement is an O.a+ cluster on this
     * device type and is created ONLY when selected through the builder —
     * with no with_*() call the mask is 0, add() refuses ("at least one O.a+
     * cluster") and the endpoint was never created. */
    {{prefix_lc}}_cfg.with_electrical_power_measurement();
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
    /* Voltage / ActiveCurrent are OPTIONAL attributes: the EPM server serves
     * them (from the delegate's getters) only when they exist on the cluster
     * at endpoint enable — esp_matter derives the server's optional-attribute
     * set from is_attribute_enabled(). Add them when the product wires the
     * matching param. */
    {{#if cfg.voltage_param}}
    {
        esp_matter::cluster_t *epm = esp_matter::cluster::get(ep, chip::app::Clusters::ElectricalPowerMeasurement::Id);
        if (!epm || !esp_matter::cluster::electrical_power_measurement::attribute::create_voltage(epm, nullable<int64_t>())) {
            ESP_LOGE(TAG, "{{prefix_lc}}: failed to add Voltage"); return ESP_FAIL;
        }
    }
    {{/if}}
    {{#if cfg.current_param}}
    {
        esp_matter::cluster_t *epm = esp_matter::cluster::get(ep, chip::app::Clusters::ElectricalPowerMeasurement::Id);
        if (!epm || !esp_matter::cluster::electrical_power_measurement::attribute::create_active_current(epm, nullable<int64_t>())) {
            ESP_LOGE(TAG, "{{prefix_lc}}: failed to add ActiveCurrent"); return ESP_FAIL;
        }
    }
    {{/if}}
}
