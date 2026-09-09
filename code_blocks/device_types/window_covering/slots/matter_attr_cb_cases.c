if (endpoint_id == s_{{prefix_lc}}_endpoint_id &&
    cluster_id == chip::app::Clusters::WindowCovering::Id &&
    attribute_id == chip::app::Clusters::WindowCovering::Attributes::TargetPositionLiftPercent100ths::Id) {
    app_driver_param_val_t pv = { .u16 = val->val.u16 };
    app_driver_set_param({{cfg.position_param}}, pv, s_handle);
}
