/* SoilMeasurement is code-driven (esp_matter 1.6): esp_matter::attribute::update()
 * has no storage for SoilMoistureMeasuredValue — the value lives in CHIP's
 * SoilMeasurementCluster, set through SetSoilMoistureMeasuredValue(), which
 * reports the change itself. It touches the data model, and driver callbacks
 * run off the Matter thread (esp_timer poll), so take the CHIP stack lock. */
if (param_id == {{cfg.value_param}}) {
    uint8_t pct = val.u8 > 100 ? 100 : val.u8;
    esp_matter::lock::ScopedChipStackLock lk(portMAX_DELAY);
    CHIP_ERROR err = chip::app::Clusters::SoilMeasurement::SetSoilMoistureMeasuredValue(
        s_{{prefix_lc}}_endpoint_id, chip::app::DataModel::Nullable<chip::Percent>(pct));
    if (err != CHIP_NO_ERROR) ESP_LOGW(TAG, "{{prefix_lc}}: soil moisture update failed: %" CHIP_ERROR_FORMAT, err.Format());
}
