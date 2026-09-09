if (endpoint == s_{{prefix_lc}}_zb_endpoint &&
    cluster_id == EZB_ZCL_CLUSTER_ID_FAN_CONTROL &&
    attr_id == EZB_ZCL_ATTR_FAN_CONTROL_FAN_MODE_ID &&
    message->in.attribute.data.type == EZB_ZCL_ATTR_TYPE_ENUM8 &&
    message->in.attribute.data.value) {
    /* FanMode → percent: off=0, low=25, medium=50, high=100, on/auto/smart=100 */
    uint8_t mode = *(uint8_t *)message->in.attribute.data.value;
    uint8_t pct = mode == EZB_ZCL_FAN_CONTROL_FAN_MODE_OFF    ? 0
                : mode == EZB_ZCL_FAN_CONTROL_FAN_MODE_LOW    ? 25
                : mode == EZB_ZCL_FAN_CONTROL_FAN_MODE_MEDIUM ? 50
                                                                 : 100;
    app_driver_param_val_t pv = { .u8 = pct };
    app_driver_set_param({{cfg.speed_param}}, pv, s_handle);
}
