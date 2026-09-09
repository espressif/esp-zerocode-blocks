if (endpoint == s_{{prefix_lc}}_zb_endpoint &&
    cluster_id == EZB_ZCL_CLUSTER_ID_ON_OFF &&
    attr_id == EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID &&
    message->in.attribute.data.type == EZB_ZCL_ATTR_TYPE_BOOL &&
    message->in.attribute.data.value) {
    app_driver_param_val_t pv = { .b = *(bool *)message->in.attribute.data.value };
    app_driver_set_param({{cfg.enabled_param}}, pv, s_handle);
}
