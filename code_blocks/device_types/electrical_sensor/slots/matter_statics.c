static uint16_t s_{{prefix_lc}}_endpoint_id = 0;

/* ElectricalPowerMeasurement is code-driven (esp_matter 1.6): every read is
 * served by CHIP's ElectricalPowerMeasurementCluster, which asks this Delegate
 * (GetActivePower(), GetVoltage(), …) — esp_matter::attribute::update() has no
 * storage to write. The delegate caches the latest readings (fed from the
 * driver params) and returns them. Voltage / ActiveCurrent are null until
 * their first reading (and unused unless the product wires voltage_param /
 * current_param, which also adds the attributes); every other measurement is
 * null; the accuracy/ranges/harmonics list iterators report "no entries". */
class {{prefix_lc}}_EPMDelegate : public chip::app::Clusters::ElectricalPowerMeasurement::Delegate
{
public:
    /* Latest active-power reading in mW, fed from the driver callback. */
    int64_t active_power_mw = 0;
    /* Latest RMS voltage (mV) / current (mA), null until first read. */
    chip::app::DataModel::Nullable<int64_t> voltage_mv;
    chip::app::DataModel::Nullable<int64_t> current_ma;

    chip::app::Clusters::ElectricalPowerMeasurement::PowerModeEnum GetPowerMode() override
        { return chip::app::Clusters::ElectricalPowerMeasurement::PowerModeEnum::kAc; }
    uint8_t GetNumberOfMeasurementTypes() override { return 1; }

    /* Accuracy / Ranges / Harmonics lists — empty (not supported). */
    CHIP_ERROR StartAccuracyRead() override { return CHIP_NO_ERROR; }
    CHIP_ERROR GetAccuracyByIndex(uint8_t, chip::app::Clusters::ElectricalPowerMeasurement::Structs::MeasurementAccuracyStruct::Type &) override
        { return CHIP_ERROR_PROVIDER_LIST_EXHAUSTED; }
    CHIP_ERROR EndAccuracyRead() override { return CHIP_NO_ERROR; }

    CHIP_ERROR StartRangesRead() override { return CHIP_NO_ERROR; }
    CHIP_ERROR GetRangeByIndex(uint8_t, chip::app::Clusters::ElectricalPowerMeasurement::Structs::MeasurementRangeStruct::Type &) override
        { return CHIP_ERROR_PROVIDER_LIST_EXHAUSTED; }
    CHIP_ERROR EndRangesRead() override { return CHIP_NO_ERROR; }

    CHIP_ERROR StartHarmonicCurrentsRead() override { return CHIP_NO_ERROR; }
    CHIP_ERROR GetHarmonicCurrentsByIndex(uint8_t, chip::app::Clusters::ElectricalPowerMeasurement::Structs::HarmonicMeasurementStruct::Type &) override
        { return CHIP_ERROR_PROVIDER_LIST_EXHAUSTED; }
    CHIP_ERROR EndHarmonicCurrentsRead() override { return CHIP_NO_ERROR; }

    CHIP_ERROR StartHarmonicPhasesRead() override { return CHIP_NO_ERROR; }
    CHIP_ERROR GetHarmonicPhasesByIndex(uint8_t, chip::app::Clusters::ElectricalPowerMeasurement::Structs::HarmonicMeasurementStruct::Type &) override
        { return CHIP_ERROR_PROVIDER_LIST_EXHAUSTED; }
    CHIP_ERROR EndHarmonicPhasesRead() override { return CHIP_NO_ERROR; }

    /* Measurement getters — ActivePower, Voltage and ActiveCurrent are real,
     * the rest are null. */
    chip::app::DataModel::Nullable<int64_t> GetVoltage() override         { return voltage_mv; }
    chip::app::DataModel::Nullable<int64_t> GetActiveCurrent() override   { return current_ma; }
    chip::app::DataModel::Nullable<int64_t> GetReactiveCurrent() override { return chip::app::DataModel::NullNullable; }
    chip::app::DataModel::Nullable<int64_t> GetApparentCurrent() override { return chip::app::DataModel::NullNullable; }
    chip::app::DataModel::Nullable<int64_t> GetActivePower() override     { return chip::app::DataModel::Nullable<int64_t>(active_power_mw); }
    chip::app::DataModel::Nullable<int64_t> GetReactivePower() override   { return chip::app::DataModel::NullNullable; }
    chip::app::DataModel::Nullable<int64_t> GetApparentPower() override   { return chip::app::DataModel::NullNullable; }
    chip::app::DataModel::Nullable<int64_t> GetRMSVoltage() override      { return chip::app::DataModel::NullNullable; }
    chip::app::DataModel::Nullable<int64_t> GetRMSCurrent() override      { return chip::app::DataModel::NullNullable; }
    chip::app::DataModel::Nullable<int64_t> GetRMSPower() override        { return chip::app::DataModel::NullNullable; }
    chip::app::DataModel::Nullable<int64_t> GetFrequency() override       { return chip::app::DataModel::NullNullable; }
    chip::app::DataModel::Nullable<int64_t> GetPowerFactor() override     { return chip::app::DataModel::NullNullable; }
    chip::app::DataModel::Nullable<int64_t> GetNeutralCurrent() override  { return chip::app::DataModel::NullNullable; }
};
static {{prefix_lc}}_EPMDelegate s_{{prefix_lc}}_epm_delegate;

/* Tell the stack an EPM attribute changed so it re-reads the delegate and
 * reports. MatterReportingAttributeChangeCallback asserts the CHIP stack lock,
 * and driver callbacks can run off the Matter thread (e.g. an esp_timer poll),
 * so take the lock (a no-op when already held). esp_matter 1.6 has no free
 * chip_stack_lock()/unlock() any more — the scoped lock is the API. */
__attribute__((unused)) static void {{prefix_lc}}_epm_report(chip::AttributeId attr)
{
    esp_matter::lock::ScopedChipStackLock lk(portMAX_DELAY);
    MatterReportingAttributeChangeCallback(s_{{prefix_lc}}_endpoint_id,
        chip::app::Clusters::ElectricalPowerMeasurement::Id, attr);
}
