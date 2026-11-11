/* A detector that published before the Matter server registered the cluster
 * was dropped by zc_smoke_co_push; replay the current state once it is up. */
if (event->Type == chip::DeviceLayer::DeviceEventType::kServerReady && s_{{prefix_lc}}_endpoint_id) {
    app_driver_param_val_t smoke = {}, co = {};
    app_driver_get_param({{cfg.smoke_state_param}}, &smoke);
    app_driver_get_param({{cfg.co_state_param}}, &co);
    zc_smoke_co_push(s_{{prefix_lc}}_endpoint_id, smoke.u8, co.u8);
}
