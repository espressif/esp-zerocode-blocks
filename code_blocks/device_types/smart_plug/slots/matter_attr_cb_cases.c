if (endpoint_id == s_{{prefix_lc}}_endpoint_id &&
    cluster_id == chip::app::Clusters::OnOff::Id &&
    attribute_id == chip::app::Clusters::OnOff::Attributes::OnOff::Id) {
    app_driver_param_val_t pval = { .b = val->val.b };
    app_driver_set_param({{cfg.power_param}}, pval, s_handle);
}
