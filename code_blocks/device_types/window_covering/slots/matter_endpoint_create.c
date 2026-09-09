{
    esp_matter::endpoint::window_covering::config_t {{prefix_lc}}_cfg;
    /* WindowCovering mandates >=1 of Lift/Tilt; default feature_flags=0 aborts at boot. */
    {{prefix_lc}}_cfg.window_covering.feature_flags |= esp_matter::cluster::window_covering::feature::lift::get_id();
    esp_matter::endpoint_t *ep = esp_matter::endpoint::window_covering::create(
        node, &{{prefix_lc}}_cfg, ENDPOINT_FLAG_NONE, NULL);
    if (!ep) { ESP_LOGE(TAG, "Failed to create {{prefix_lc}} endpoint"); return ESP_FAIL; }
    s_{{prefix_lc}}_endpoint_id = esp_matter::endpoint::get_id(ep);
    ESP_LOGI(TAG, "{{prefix_lc}}: window_covering endpoint id=%d", s_{{prefix_lc}}_endpoint_id);
    /* NVS wear: position updates on every step of motor travel — defer. */
    {
        esp_matter::attribute_t *a = esp_matter::attribute::get(
            s_{{prefix_lc}}_endpoint_id, chip::app::Clusters::WindowCovering::Id,
            chip::app::Clusters::WindowCovering::Attributes::CurrentPositionLiftPercent100ths::Id);
        if (a) esp_matter::attribute::set_deferred_persistence(a);
    }
}
