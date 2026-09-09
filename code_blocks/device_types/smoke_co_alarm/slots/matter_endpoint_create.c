{
    esp_matter::endpoint::smoke_co_alarm::config_t {{prefix_lc}}_cfg;
    /* SmokeCOAlarm mandates >=1 of Smoke/CO; default feature_flags=0 aborts at boot. */
    {{prefix_lc}}_cfg.smoke_co_alarm.feature_flags |= esp_matter::cluster::smoke_co_alarm::feature::smoke_alarm::get_id();
    esp_matter::endpoint_t *ep = esp_matter::endpoint::smoke_co_alarm::create(
        node, &{{prefix_lc}}_cfg, ENDPOINT_FLAG_NONE, NULL);
    if (!ep) { ESP_LOGE(TAG, "Failed to create {{prefix_lc}} endpoint"); return ESP_FAIL; }
    s_{{prefix_lc}}_endpoint_id = esp_matter::endpoint::get_id(ep);
    ESP_LOGI(TAG, "{{prefix_lc}}: smoke_co_alarm endpoint id=%d", s_{{prefix_lc}}_endpoint_id);
}
