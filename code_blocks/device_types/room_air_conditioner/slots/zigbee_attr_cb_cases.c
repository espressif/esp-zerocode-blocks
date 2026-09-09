if (endpoint == s_{{prefix_lc}}_zb_endpoint) {
    if (cluster_id == EZB_ZCL_CLUSTER_ID_ON_OFF &&
        attr_id == EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID &&
        message->in.attribute.data.type == EZB_ZCL_ATTR_TYPE_BOOL &&
        message->in.attribute.data.value) {
        app_driver_param_val_t pv = { .b = *(bool *)message->in.attribute.data.value };
        app_driver_set_param({{cfg.power_param}}, pv, s_handle);
    } else if (cluster_id == EZB_ZCL_CLUSTER_ID_THERMOSTAT &&
               attr_id == EZB_ZCL_ATTR_THERMOSTAT_OCCUPIED_COOLING_SETPOINT_ID &&
               message->in.attribute.data.type == EZB_ZCL_ATTR_TYPE_INT16 &&
               message->in.attribute.data.value) {
        app_driver_param_val_t pv = { .i16 = *(int16_t *)message->in.attribute.data.value };
        app_driver_set_param({{cfg.cool_setpoint_param}}, pv, s_handle);
    } else if (cluster_id == EZB_ZCL_CLUSTER_ID_FAN_CONTROL &&
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
        app_driver_set_param({{cfg.fan_speed_param}}, pv, s_handle);
    }
}
