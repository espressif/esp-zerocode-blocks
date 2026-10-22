{
    esp_matter::endpoint::room_air_conditioner::config_t {{prefix_lc}}_cfg;
    /* Thermostat cluster mandates >=1 of Heat/Cool; default feature_flags=0 aborts at boot. AC cools. */
    {{prefix_lc}}_cfg.thermostat.feature_flags |= esp_matter::cluster::thermostat::feature::cooling::get_id();
    esp_matter::endpoint_t *ep = esp_matter::endpoint::room_air_conditioner::create(
        node, &{{prefix_lc}}_cfg, ENDPOINT_FLAG_NONE, NULL);
    if (!ep) { ESP_LOGE(TAG, "Failed to create {{prefix_lc}} endpoint"); return ESP_FAIL; }
    s_{{prefix_lc}}_endpoint_id = esp_matter::endpoint::get_id(ep);
    ESP_LOGI(TAG, "{{prefix_lc}}: room_air_conditioner endpoint id=%d", s_{{prefix_lc}}_endpoint_id);
    /* FanControl is OPTIONAL on Room Air Conditioner and esp_matter's
     * room_air_conditioner::create() does not add it (descriptor, identify,
     * on_off, thermostat only) — without it every fan_speed_param update and
     * controller write had no cluster to land on. Default config = the fan
     * device type's (FanModeSequence OffLowMedHigh). */
    {
        esp_matter::cluster::fan_control::config_t fan_cfg;
        if (!esp_matter::cluster::fan_control::create(ep, &fan_cfg, CLUSTER_FLAG_SERVER)) {
            ESP_LOGE(TAG, "Failed to create {{prefix_lc}} FanControl cluster");
            return ESP_FAIL;
        }
    }
}
