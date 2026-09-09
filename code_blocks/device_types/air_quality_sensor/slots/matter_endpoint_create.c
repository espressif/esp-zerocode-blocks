{
    esp_matter::endpoint::air_quality_sensor::config_t {{prefix_lc}}_cfg;
    esp_matter::endpoint_t *ep = esp_matter::endpoint::air_quality_sensor::create(
        node, &{{prefix_lc}}_cfg, ENDPOINT_FLAG_NONE, NULL);
    if (!ep) { ESP_LOGE(TAG, "Failed to create {{prefix_lc}} endpoint"); return ESP_FAIL; }
    s_{{prefix_lc}}_endpoint_id = esp_matter::endpoint::get_id(ep);
    ESP_LOGI(TAG, "{{prefix_lc}}: air_quality_sensor endpoint id=%d", s_{{prefix_lc}}_endpoint_id);
    {{#if cfg.co2_ppm_param}}
    /* CO2 concentration cluster — enable NumericMeasurement BEFORE create or
     * it aborts at boot ("at least one of NumericMeasurement, LevelIndication"). */
    {
        esp_matter::cluster::concentration_measurement::config_t co2_cfg;
        co2_cfg.measurement_medium = 0; /* Air */
        co2_cfg.feature_flags = esp_matter::cluster::concentration_measurement::feature::numeric_measurement::get_id();
        esp_matter::cluster_t *co2_c = esp_matter::cluster::carbon_dioxide_concentration_measurement::create(
            ep, &co2_cfg, CLUSTER_FLAG_SERVER);
        if (!co2_c) { ESP_LOGE(TAG, "Failed to create {{prefix_lc}} CO2 cluster"); return ESP_FAIL; }
    }
    {{/if}}
    {{#if cfg.pm25_param}}
    {
        esp_matter::cluster::concentration_measurement::config_t pm25_cfg;
        pm25_cfg.measurement_medium = 0; /* Air */
        pm25_cfg.feature_flags = esp_matter::cluster::concentration_measurement::feature::numeric_measurement::get_id();
        esp_matter::cluster_t *pm25_c = esp_matter::cluster::pm25_concentration_measurement::create(
            ep, &pm25_cfg, CLUSTER_FLAG_SERVER);
        if (!pm25_c) { ESP_LOGE(TAG, "Failed to create {{prefix_lc}} PM2.5 cluster"); return ESP_FAIL; }
    }
    {{/if}}
    {{#if cfg.tvoc_param}}
    {
        esp_matter::cluster::concentration_measurement::config_t tvoc_cfg;
        tvoc_cfg.measurement_medium = 0; /* Air */
        tvoc_cfg.feature_flags = esp_matter::cluster::concentration_measurement::feature::numeric_measurement::get_id();
        esp_matter::cluster_t *tvoc_c = esp_matter::cluster::total_volatile_organic_compounds_concentration_measurement::create(
            ep, &tvoc_cfg, CLUSTER_FLAG_SERVER);
        if (!tvoc_c) { ESP_LOGE(TAG, "Failed to create {{prefix_lc}} TVOC cluster"); return ESP_FAIL; }
    }
    {{/if}}
}
