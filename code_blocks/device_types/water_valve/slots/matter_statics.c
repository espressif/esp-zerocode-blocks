static uint16_t s_{{prefix_lc}}_endpoint_id = 0;

/* Push the physical state into the cluster OBJECT. attribute::update() cannot
 * write a code-driven cluster — collapsible to it once esp-matter#1798 lands.
 * MUST run on the Matter thread. */
static void {{prefix_lc}}_valve_update_cluster(uint16_t ep, bool open)
{
    chip::app::ServerClusterInterface *iface =
        esp_matter::data_model::provider::get_instance().registry().Get(
            chip::app::ConcreteClusterPath(ep, chip::app::Clusters::ValveConfigurationAndControl::Id));
    if (!iface) {
        ESP_LOGW(TAG, "{{prefix_lc}}: valve cluster not registered (ep %u)", ep);
        return;
    }
    /* Sound cast: esp_matter's integration registers exactly this type. */
    auto *cluster = static_cast<chip::app::Clusters::ValveConfigurationAndControlCluster *>(iface);
    cluster->UpdateCurrentState(open
        ? chip::app::Clusters::ValveConfigurationAndControl::ValveStateEnum::kOpen
        : chip::app::Clusters::ValveConfigurationAndControl::ValveStateEnum::kClosed);
}

/* ScheduleWork trampoline for state changes born OFF the Matter thread
 * (button, console, RainMaker). */
static void {{prefix_lc}}_valve_push_state(intptr_t arg)
{
    {{prefix_lc}}_valve_update_cluster((uint16_t)(arg >> 1), (arg & 1) != 0);
}

/* Controller -> hardware. The cluster invokes this delegate for Open/Close on
 * the Matter thread, sets CurrentState=Transitioning, and waits for the app to
 * confirm — so after driving the relay (instant), confirm DIRECTLY here. The
 * confirmation cannot ride the driver callback: source is s_handle so the bus
 * (correctly) never echoes back to this solution, and a redundant command
 * (Open while open) is deduped by set_param yet still needs its confirmation
 * or the cluster reports Transitioning forever. */
class {{prefix_lc}}_valve_delegate_t : public chip::app::Clusters::ValveConfigurationAndControl::Delegate {
public:
    chip::app::DataModel::Nullable<chip::Percent> HandleOpenValve(
        chip::app::DataModel::Nullable<chip::Percent> level) override
    {
        app_driver_param_val_t v = { .b = true };
        app_driver_set_param({{cfg.power_param}}, v, s_handle);
        {{prefix_lc}}_valve_update_cluster(s_{{prefix_lc}}_endpoint_id, true);
        (void)level; /* LVL feature not enabled — binary valve */
        return chip::app::DataModel::NullNullable;
    }
    CHIP_ERROR HandleCloseValve() override
    {
        app_driver_param_val_t v = { .b = false };
        app_driver_set_param({{cfg.power_param}}, v, s_handle);
        {{prefix_lc}}_valve_update_cluster(s_{{prefix_lc}}_endpoint_id, false);
        return CHIP_NO_ERROR;
    }
    void HandleRemainingDurationTick(uint32_t) override {}
};
static {{prefix_lc}}_valve_delegate_t s_{{prefix_lc}}_valve_delegate;
