static uint16_t s_{{prefix_lc}}_endpoint_id = 0;

/* ElectricalPowerMeasurement::ActivePower is ATTRIBUTE_FLAG_MANAGED_INTERNALLY
 * in esp_matter 1.4.2: the cluster has no server-side storage for it, so
 * esp_matter::attribute::update() -> emberAfWriteAttribute() fails and crashes
 * at boot. The value MUST be supplied on demand by a cluster Delegate; the EPM
 * server plugin calls GetActivePower() whenever an Attribute read/report happens.
 * The delegate caches the latest reading (fed from the driver param) and returns
 * it. All other measurement getters are null (only active power is measured);
 * the accuracy/ranges/harmonics list iterators report "no entries". */
class {{prefix_lc}}_EPMDelegate : public chip::app::Clusters::ElectricalPowerMeasurement::Delegate
{
public:
    /* Latest active-power reading in mW, fed from the driver callback. */
    int64_t active_power_mw = 0;

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

    /* Measurement getters — only ActivePower is real, the rest are null. */
    chip::app::DataModel::Nullable<int64_t> GetVoltage() override         { return chip::app::DataModel::NullNullable; }
    chip::app::DataModel::Nullable<int64_t> GetActiveCurrent() override   { return chip::app::DataModel::NullNullable; }
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
