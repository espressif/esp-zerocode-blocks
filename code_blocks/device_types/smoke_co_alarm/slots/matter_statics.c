static uint16_t s_{{prefix_lc}}_endpoint_id = 0;

#ifndef ZC_SMOKE_CO_ALARM_HELPERS
#define ZC_SMOKE_CO_ALARM_HELPERS
/* esp_matter 1.6 registers a CHIP SmokeCoAlarmCluster per endpoint and serves
 * every read from it, so esp_matter::attribute::update() only writes a shadow
 * nobody reads. The cluster's own setters are the only path that changes what
 * a controller sees AND emits the SmokeAlarm / COAlarm events and the
 * ExpressedState that Apple/Google Home alert on. They assert the CHIP stack
 * lock, so callers hold it. Returns NULL before the Matter server has
 * registered the cluster (early boot). */
static chip::app::Clusters::SmokeCoAlarmCluster *zc_smoke_co_cluster(uint16_t ep)
{
    chip::app::ServerClusterInterface *c = esp_matter::data_model::provider::get_instance().registry().Get(
        chip::app::ConcreteClusterPath(ep, chip::app::Clusters::SmokeCoAlarm::Id));
    return static_cast<chip::app::Clusters::SmokeCoAlarmCluster *>(c);
}

/* Highest priority first — the order CHIP's reference smoke/CO app uses. */
static const std::array<chip::app::Clusters::SmokeCoAlarm::ExpressedStateEnum,
                        chip::app::Clusters::SmokeCoAlarmCluster::kPriorityOrderLength> kZcSmokeCoPriority = {
    chip::app::Clusters::SmokeCoAlarm::ExpressedStateEnum::kInoperative,
    chip::app::Clusters::SmokeCoAlarm::ExpressedStateEnum::kSmokeAlarm,
    chip::app::Clusters::SmokeCoAlarm::ExpressedStateEnum::kInterconnectSmoke,
    chip::app::Clusters::SmokeCoAlarm::ExpressedStateEnum::kCOAlarm,
    chip::app::Clusters::SmokeCoAlarm::ExpressedStateEnum::kInterconnectCO,
    chip::app::Clusters::SmokeCoAlarm::ExpressedStateEnum::kHardwareFault,
    chip::app::Clusters::SmokeCoAlarm::ExpressedStateEnum::kTesting,
    chip::app::Clusters::SmokeCoAlarm::ExpressedStateEnum::kEndOfService,
    chip::app::Clusters::SmokeCoAlarm::ExpressedStateEnum::kBatteryAlert,
};

/* Push one endpoint's smoke + CO state into the cluster. Takes the stack lock
 * (scoped; a no-op when the caller already holds it, e.g. on the Matter thread). */
static void zc_smoke_co_push(uint16_t ep, uint8_t smoke, uint8_t co)
{
    using chip::app::Clusters::SmokeCoAlarm::AlarmStateEnum;
    esp_matter::lock::ScopedChipStackLock lk(portMAX_DELAY);
    chip::app::Clusters::SmokeCoAlarmCluster *c = zc_smoke_co_cluster(ep);
    if (c) {
        c->SetSmokeState(static_cast<AlarmStateEnum>(smoke > 2 ? 2 : smoke));
        c->SetCOState(static_cast<AlarmStateEnum>(co > 2 ? 2 : co));
        c->SetExpressedStateByPriority(kZcSmokeCoPriority);
    }
}
#endif
