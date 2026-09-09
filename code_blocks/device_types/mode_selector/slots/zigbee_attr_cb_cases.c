if (endpoint == s_{{prefix_lc}}_zb_endpoint &&
    cluster_id == EZB_ZCL_CLUSTER_ID_MULTISTATE_VALUE &&
    attr_id == EZB_ZCL_ATTR_MULTISTATE_VALUE_PRESENT_VALUE_ID &&
    message->in.attribute.data.type == EZB_ZCL_ATTR_TYPE_UINT16 &&
    message->in.attribute.data.value) {
    uint16_t mode = *(uint16_t *)message->in.attribute.data.value;
    if (mode > {{cfg.mode_count}} - 1) mode = {{cfg.mode_count}} - 1;
    app_driver_param_val_t pv = { .u8 = (uint8_t)mode };
    app_driver_set_param({{cfg.mode_param}}, pv, s_handle);
}
