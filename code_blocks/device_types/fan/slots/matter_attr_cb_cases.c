if (endpoint_id == s_{{prefix_lc}}_endpoint_id &&
    cluster_id == chip::app::Clusters::FanControl::Id &&
    attribute_id == chip::app::Clusters::FanControl::Attributes::PercentSetting::Id) {
    app_driver_param_val_t pv = { .u8 = val->val.u8 };
    app_driver_set_param({{cfg.speed_param}}, pv, s_handle);
}
