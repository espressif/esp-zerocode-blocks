if (param_id == {{cfg.power_param}}) {
    CHIP_ERROR sw_err = chip::DeviceLayer::PlatformMgr().ScheduleWork(
        {{prefix_lc}}_valve_push_state,
        (intptr_t)(((intptr_t)s_{{prefix_lc}}_endpoint_id << 1) | (val.b ? 1 : 0)));
    if (sw_err != CHIP_NO_ERROR) {
        ESP_LOGW(TAG, "{{prefix_lc}}: valve state report not scheduled, err:%" CHIP_ERROR_FORMAT, sw_err.Format());
    }
}
