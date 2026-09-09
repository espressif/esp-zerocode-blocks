/* Report ONLY ActivePower. esp_matter 1.4.2's ElectricalPowerMeasurement
 * does NOT declare a `Current` (or `Voltage`) attribute — referencing
 * `...Attributes::Current::Id` does not compile. Do not add other attribute
 * updates to this cluster here. (voltage_param/current_param are NOT wired
 * to Matter; the RainMaker binding below reports them when configured.) */
if (param_id == {{cfg.active_power_param}}) {
    s_from_driver = true;
    /* ActivePower is MANAGED_INTERNALLY: the cluster keeps NO server-side
     * copy, so esp_matter::attribute::update() -> emberAfWriteAttribute()
     * fails and crashes at boot. Instead, hand the new value to the Delegate
     * (the EPM server reads it back via GetActivePower()) and tell the stack
     * the attribute changed so it re-reads + reports.
     * MatterReportingAttributeChangeCallback asserts the CHIP stack lock, and
     * driver callbacks can run off the Matter thread (e.g. an esp_timer poll),
     * so take the lock here. */
    s_{{prefix_lc}}_epm_delegate.active_power_mw = (int64_t)val.u32;
    esp_matter::lock::status_t {{prefix_lc}}_lk = esp_matter::lock::chip_stack_lock(portMAX_DELAY);
    if ({{prefix_lc}}_lk != esp_matter::lock::FAILED) {
        MatterReportingAttributeChangeCallback(s_{{prefix_lc}}_endpoint_id,
            chip::app::Clusters::ElectricalPowerMeasurement::Id,
            chip::app::Clusters::ElectricalPowerMeasurement::Attributes::ActivePower::Id);
        if ({{prefix_lc}}_lk == esp_matter::lock::SUCCESS) {
            esp_matter::lock::chip_stack_unlock();
        }
    }
    s_from_driver = false;
}
