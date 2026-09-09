if (endpoint == s_{{prefix_lc}}_zb_endpoint && message->in.attribute.data.value) {
    if (cluster_id == EZB_ZCL_CLUSTER_ID_ON_OFF &&
        attr_id == EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID &&
        message->in.attribute.data.type == EZB_ZCL_ATTR_TYPE_BOOL) {
        app_driver_param_val_t pv = { .b = *(bool *)message->in.attribute.data.value };
        app_driver_set_param({{cfg.power_param}}, pv, s_handle);
    } else if (cluster_id == EZB_ZCL_CLUSTER_ID_LEVEL &&
               attr_id == EZB_ZCL_ATTR_LEVEL_CURRENT_LEVEL_ID &&
               message->in.attribute.data.type == EZB_ZCL_ATTR_TYPE_UINT8) {
        /* ZCL CurrentLevel and the driver brightness are both 0-254. */
        app_driver_param_val_t pv = { .u8 = *(uint8_t *)message->in.attribute.data.value };
        app_driver_set_param({{cfg.brightness_param}}, pv, s_handle);
    }
}
