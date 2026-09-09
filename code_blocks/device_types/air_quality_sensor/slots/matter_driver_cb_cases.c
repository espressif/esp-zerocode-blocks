/* attribute::update() takes the CHIP stack lock internally — safe to call here even though driver callbacks run off the Matter event-loop thread. If you extend this block, keep using attribute::update; do NOT swap in a raw *Server::Set* cluster setter from a poll timer / console / ISR (no lock → AssertChipStackLockedByCurrentThread abort). Wrap a raw setter in esp_matter::lock::chip_stack_lock() or PlatformMgr().ScheduleWork() only if truly needed. */
if (param_id == {{cfg.air_quality_param}}) {
    s_from_driver = true;
    esp_matter_attr_val_t mval = esp_matter_enum8(val.u8);
    esp_matter::attribute::update(s_{{prefix_lc}}_endpoint_id,
        chip::app::Clusters::AirQuality::Id,
        chip::app::Clusters::AirQuality::Attributes::AirQuality::Id, &mval);
    s_from_driver = false;
}
{{#if cfg.co2_ppm_param}}
if (param_id == {{cfg.co2_ppm_param}}) {
    s_from_driver = true;
    /* MeasuredValue is a nullable<float>; the driver carries ppm as u16. */
    esp_matter_attr_val_t mval = esp_matter_nullable_float(nullable<float>((float)val.u16));
    esp_err_t err = esp_matter::attribute::update(s_{{prefix_lc}}_endpoint_id,
        chip::app::Clusters::CarbonDioxideConcentrationMeasurement::Id,
        chip::app::Clusters::CarbonDioxideConcentrationMeasurement::Attributes::MeasuredValue::Id, &mval);
    if (err != ESP_OK) ESP_LOGW(TAG, "{{prefix_lc}}: CO2 update failed: %s", esp_err_to_name(err));
    s_from_driver = false;
}
{{/if}}
{{#if cfg.pm25_param}}
if (param_id == {{cfg.pm25_param}}) {
    s_from_driver = true;
    esp_matter_attr_val_t mval = esp_matter_nullable_float(nullable<float>((float)val.u16));
    esp_err_t err = esp_matter::attribute::update(s_{{prefix_lc}}_endpoint_id,
        chip::app::Clusters::Pm25ConcentrationMeasurement::Id,
        chip::app::Clusters::Pm25ConcentrationMeasurement::Attributes::MeasuredValue::Id, &mval);
    if (err != ESP_OK) ESP_LOGW(TAG, "{{prefix_lc}}: PM2.5 update failed: %s", esp_err_to_name(err));
    s_from_driver = false;
}
{{/if}}
{{#if cfg.tvoc_param}}
if (param_id == {{cfg.tvoc_param}}) {
    s_from_driver = true;
    esp_matter_attr_val_t mval = esp_matter_nullable_float(nullable<float>((float)val.u16));
    esp_err_t err = esp_matter::attribute::update(s_{{prefix_lc}}_endpoint_id,
        chip::app::Clusters::TotalVolatileOrganicCompoundsConcentrationMeasurement::Id,
        chip::app::Clusters::TotalVolatileOrganicCompoundsConcentrationMeasurement::Attributes::MeasuredValue::Id, &mval);
    if (err != ESP_OK) ESP_LOGW(TAG, "{{prefix_lc}}: TVOC update failed: %s", esp_err_to_name(err));
    s_from_driver = false;
}
{{/if}}
