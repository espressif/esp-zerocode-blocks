if (param_id == {{cfg.state_param}}) {
    CHIP_ERROR sw = chip::DeviceLayer::PlatformMgr().ScheduleWork(
        zc::operational_state_push, zc::pack_ep_u8(s_{{prefix_lc}}_endpoint_id, val.u8));
    if (sw != CHIP_NO_ERROR) ESP_LOGW(TAG, "{{prefix_lc}}: opstate report not scheduled");
}
{{#if cfg.mode_param}}
if (param_id == {{cfg.mode_param}}) {
    CHIP_ERROR sw = chip::DeviceLayer::PlatformMgr().ScheduleWork(
        zc::mode_push<chip::app::Clusters::LaundryWasherMode::Id>,
        zc::pack_ep_u8(s_{{prefix_lc}}_endpoint_id, val.u8));
    if (sw != CHIP_NO_ERROR) ESP_LOGW(TAG, "{{prefix_lc}}: mode report not scheduled");
}
{{/if}}
