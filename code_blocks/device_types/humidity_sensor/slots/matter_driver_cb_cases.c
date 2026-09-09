/* attribute::update() takes the CHIP stack lock internally — safe to call here even though driver callbacks run off the Matter event-loop thread. If you extend this block, keep using attribute::update; do NOT swap in a raw *Server::Set* cluster setter from a poll timer / console / ISR (no lock → AssertChipStackLockedByCurrentThread abort). Wrap a raw setter in esp_matter::lock::chip_stack_lock() or PlatformMgr().ScheduleWork() only if truly needed. */
if (param_id == {{cfg.value_param}}) {
    s_from_driver = true;
    esp_matter_attr_val_t mval = esp_matter_nullable_uint16((uint16_t)val.i16);
    esp_err_t err = esp_matter::attribute::update(
        s_{{prefix_lc}}_endpoint_id,
        chip::app::Clusters::RelativeHumidityMeasurement::Id,
        chip::app::Clusters::RelativeHumidityMeasurement::Attributes::MeasuredValue::Id,
        &mval);
    if (err != ESP_OK) ESP_LOGW(TAG, "{{prefix_lc}}: humidity update failed: %s", esp_err_to_name(err));
    s_from_driver = false;
}
