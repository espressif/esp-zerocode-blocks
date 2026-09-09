if (endpoint == s_{{prefix_lc}}_zb_endpoint &&
    cluster_id == EZB_ZCL_CLUSTER_ID_THERMOSTAT) {
    if (attr_id == EZB_ZCL_ATTR_THERMOSTAT_OCCUPIED_HEATING_SETPOINT_ID &&
        message->in.attribute.data.type == EZB_ZCL_ATTR_TYPE_INT16 &&
        message->in.attribute.data.value) {
        app_driver_param_val_t pv = { .i16 = *(int16_t *)message->in.attribute.data.value };
        app_driver_set_param({{cfg.heat_setpoint_param}}, pv, s_handle);
    } else if (attr_id == EZB_ZCL_ATTR_THERMOSTAT_SYSTEM_MODE_ID &&
               message->in.attribute.data.type == EZB_ZCL_ATTR_TYPE_ENUM8 &&
               message->in.attribute.data.value) {
        app_driver_param_val_t pv = { .u8 = *(uint8_t *)message->in.attribute.data.value };
        app_driver_set_param({{cfg.system_mode_param}}, pv, s_handle);
    }
}
