{
    esp_matter::endpoint::water_heater::config_t {{prefix_lc}}_cfg;
    /* Thermostat cluster mandates >=1 of Heat/Cool; default feature_flags=0 aborts at boot. */
    {{prefix_lc}}_cfg.thermostat.feature_flags |= esp_matter::cluster::thermostat::feature::heating::get_id();
    /* ControlSequenceOfOperation defaults to 0 = CoolingOnly, under which CHIP
     * refuses SystemMode=Heat. A water heater only heats: 2 = HeatingOnly. */
    {{prefix_lc}}_cfg.thermostat.control_sequence_of_operation = 2;
    {
        /* The water_heater device type has NO OnOff cluster: power is
         * Thermostat SystemMode (0 = Off, 4 = Heat). Start from the driver. */
        app_driver_param_val_t init = {};
        app_driver_get_param({{cfg.power_param}}, &init);
        {{prefix_lc}}_cfg.thermostat.system_mode = init.b ? 4 : 0;
        app_driver_get_param({{cfg.setpoint_param}}, &init);
        if (init.i16 >= 700 && init.i16 <= 8000)
            {{prefix_lc}}_cfg.thermostat.features.heating.occupied_heating_setpoint = init.i16;
    }
    esp_matter::endpoint_t *ep = esp_matter::endpoint::water_heater::create(
        node, &{{prefix_lc}}_cfg, ENDPOINT_FLAG_NONE, NULL);
    if (!ep) { ESP_LOGE(TAG, "Failed to create {{prefix_lc}} endpoint"); return ESP_FAIL; }
    s_{{prefix_lc}}_endpoint_id = esp_matter::endpoint::get_id(ep);
    {
        /* Without AbsMax/MaxHeatSetpointLimit CHIP falls back to 30.00 °C and
         * rejects every hotter OccupiedHeatingSetpoint — i.e. every useful
         * water temperature, including the driver's own push. Allow up to
         * 80 °C, the same ceiling the RainMaker binding offers. */
        esp_matter::cluster_t *th = esp_matter::cluster::get(ep, chip::app::Clusters::Thermostat::Id);
        if (th) {
            esp_matter::cluster::thermostat::attribute::create_abs_max_heat_setpoint_limit(th, 8000);
            esp_matter::cluster::thermostat::attribute::create_max_heat_setpoint_limit(th, 8000);
        }
    }
    ESP_LOGI(TAG, "{{prefix_lc}}: water_heater endpoint id=%d", s_{{prefix_lc}}_endpoint_id);
}
