{
    esp_matter::endpoint::soil_sensor::config_t {{prefix_lc}}_cfg;
    esp_matter::endpoint_t *ep = esp_matter::endpoint::soil_sensor::create(
        node, &{{prefix_lc}}_cfg, ENDPOINT_FLAG_NONE, NULL);
    if (!ep) { ESP_LOGE(TAG, "Failed to create {{prefix_lc}} endpoint"); return ESP_FAIL; }
    s_{{prefix_lc}}_endpoint_id = esp_matter::endpoint::get_id(ep);
    /* SoilMeasurement is code-driven: its server is built at esp_matter::start()
     * from the limits registered here, and the init callback ABORTS
     * ("Please set the limit for SoilMeasurementCluster") when none were set.
     * The value is whole percent, 0-100. */
    {
        chip::app::Clusters::SoilMeasurement::Attributes::SoilMoistureMeasurementLimits::TypeInfo::Type limits;
        limits.measurementType = chip::app::Clusters::Globals::MeasurementTypeEnum::kSoilMoisture;
        limits.measured = true;
        limits.minMeasuredValue = 0;
        limits.maxMeasuredValue = 100;
        chip::app::Clusters::SoilMeasurement::SetSoilMoistureLimits(s_{{prefix_lc}}_endpoint_id, limits);
    }
    ESP_LOGI(TAG, "{{prefix_lc}}: soil_sensor endpoint id=%d", s_{{prefix_lc}}_endpoint_id);
}
