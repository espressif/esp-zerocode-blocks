static uint16_t s_{{prefix_lc}}_endpoint_id = 0;

#ifndef ZC_AIR_QUALITY_CONC_HELPERS
#define ZC_AIR_QUALITY_CONC_HELPERS
/* esp_matter 1.6 creates CO2 and TVOC concentration attributes as
 * MANAGED_INTERNALLY (served by a CHIP ServerCluster object) but ships no
 * integration that constructs one — unlike flow/illuminance — so without this
 * every read failed and every attribute::update() returned NOT_SUPPORTED.
 * The cluster's init/shutdown callbacks (set in matter_endpoint_create) build
 * and register CHIP's ConcentrationMeasurementCluster, mirroring esp_matter's
 * own integration.cpp pattern. The UNIT is fixed here per gas: CO2 in ppm,
 * TVOC in ppb (block.yml driver_params; HA advertises the same). Min 0 (a
 * concentration cannot be negative), Max null: the range is the sensor's. */
static std::map<std::pair<chip::EndpointId, chip::ClusterId>,
                chip::app::LazyRegisteredServerCluster<
                    chip::app::Clusters::ConcentrationMeasurement::ConcentrationMeasurementCluster>> s_zc_aq_conc;

__attribute__((unused)) static void zc_aq_conc_init(uint16_t ep, chip::ClusterId id,
                            chip::app::Clusters::ConcentrationMeasurement::MeasurementUnitEnum unit)
{
    using namespace chip::app::Clusters::ConcentrationMeasurement;
    auto &srv = s_zc_aq_conc[{ ep, id }];
    if (!srv.IsConstructed()) {
        ConcentrationMeasurementCluster::Config cfg;
        cfg.clusterId = id;
        cfg.features = chip::BitFlags<Feature>(Feature::kNumericMeasurement);
        cfg.medium = MeasurementMediumEnum::kAir;
        cfg.unit = unit;
        cfg.minMeasured = chip::app::DataModel::MakeNullable(0.0f);
        cfg.maxMeasured = chip::app::DataModel::Nullable<float>();
        srv.Create(ep, cfg);
    }
    CHIP_ERROR err = esp_matter::data_model::provider::get_instance().registry().Register(srv.Registration());
    if (err != CHIP_NO_ERROR) {
        ESP_LOGE(TAG, "concentration 0x%04" PRIx32 " register failed ep %u: %" CHIP_ERROR_FORMAT,
                 (uint32_t)id, ep, err.Format());
    }
}

__attribute__((unused)) static void zc_aq_conc_shutdown(uint16_t ep, chip::ClusterId id, chip::app::ClusterShutdownType type)
{
    auto it = s_zc_aq_conc.find({ ep, id });
    if (it == s_zc_aq_conc.end() || !it->second.IsConstructed()) return;
    RETURN_SAFELY_IGNORED esp_matter::data_model::provider::get_instance().registry().Unregister(&it->second.Cluster(), type);
    if (type == chip::app::ClusterShutdownType::kPermanentRemove) {
        it->second.Destroy();
        s_zc_aq_conc.erase(it);
    }
}

__attribute__((unused)) static void zc_aq_co2_init(uint16_t ep)
{
    zc_aq_conc_init(ep, chip::app::Clusters::CarbonDioxideConcentrationMeasurement::Id,
                    chip::app::Clusters::ConcentrationMeasurement::MeasurementUnitEnum::kPpm);
}
__attribute__((unused)) static void zc_aq_co2_shutdown(uint16_t ep, chip::app::ClusterShutdownType type)
{
    zc_aq_conc_shutdown(ep, chip::app::Clusters::CarbonDioxideConcentrationMeasurement::Id, type);
}
__attribute__((unused)) static void zc_aq_tvoc_init(uint16_t ep)
{
    zc_aq_conc_init(ep, chip::app::Clusters::TotalVolatileOrganicCompoundsConcentrationMeasurement::Id,
                    chip::app::Clusters::ConcentrationMeasurement::MeasurementUnitEnum::kPpb);
}
__attribute__((unused)) static void zc_aq_tvoc_shutdown(uint16_t ep, chip::app::ClusterShutdownType type)
{
    zc_aq_conc_shutdown(ep, chip::app::Clusters::TotalVolatileOrganicCompoundsConcentrationMeasurement::Id, type);
}

/* Push one reading. The cluster's setters assert the CHIP stack lock; the
 * scoped lock is a no-op when the caller already holds it. Before the server
 * has registered (early boot) the reading is dropped; the next one lands. */
__attribute__((unused)) static void zc_aq_conc_set(uint16_t ep, chip::ClusterId id, float value)
{
    esp_matter::lock::ScopedChipStackLock lk(portMAX_DELAY);
    auto it = s_zc_aq_conc.find({ ep, id });
    if (it == s_zc_aq_conc.end() || !it->second.IsConstructed()) return;
    CHIP_ERROR err = it->second.Cluster().SetMeasuredValue(chip::app::DataModel::MakeNullable(value));
    if (err != CHIP_NO_ERROR) {
        ESP_LOGW(TAG, "concentration 0x%04" PRIx32 " update failed: %" CHIP_ERROR_FORMAT, (uint32_t)id, err.Format());
    }
}
#endif
