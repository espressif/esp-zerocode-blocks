{
    esp_matter::endpoint::window_covering::config_t {{prefix_lc}}_cfg;
    /* WindowCovering mandates >=1 of Lift/Tilt; default feature_flags=0 aborts at boot.
     * PositionAwareLift too: without it esp_matter creates neither
     * TargetPositionLiftPercent100ths nor CurrentPositionLiftPercent100ths, and
     * CHIP never writes Target on UpOrOpen / DownOrClose / GoToLiftPercentage —
     * so no Matter command would ever reach the driver. */
    {{prefix_lc}}_cfg.window_covering.feature_flags |=
        esp_matter::cluster::window_covering::feature::lift::get_id() |
        esp_matter::cluster::window_covering::feature::position_aware_lift::get_id();
    /* Start from where the driver says the blind is. Matter counts percent
     * CLOSED (0 = open, 10000 = closed); the driver counts percent OPEN. */
    {
        app_driver_param_val_t init = {};
        app_driver_get_param({{cfg.position_param}}, &init);
        uint16_t closed = (uint16_t)(10000 - (init.u16 > 10000 ? 10000 : init.u16));
        {{prefix_lc}}_cfg.window_covering.features.position_aware_lift.current_position_lift_percent_100ths = closed;
        {{prefix_lc}}_cfg.window_covering.features.position_aware_lift.target_position_lift_percent_100ths = closed;
    }
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
