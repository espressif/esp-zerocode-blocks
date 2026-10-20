if (endpoint_id == s_{{prefix_lc}}_endpoint_id &&
    cluster_id == chip::app::Clusters::WindowCovering::Id &&
    attribute_id == chip::app::Clusters::WindowCovering::Attributes::TargetPositionLiftPercent100ths::Id &&
    val->val.u16 <= 10000) {
    /* Matter Target is percent CLOSED (0 = open); the driver param is percent
     * OPEN. A null Target (0xFFFF) is "no target" and is ignored. */
    app_driver_param_val_t pv = { .u16 = (uint16_t)(10000 - val->val.u16) };
    app_driver_set_param({{cfg.position_param}}, pv, s_handle);
}
