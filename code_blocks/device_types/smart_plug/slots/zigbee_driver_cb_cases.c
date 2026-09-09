if (param_id == {{cfg.power_param}} && s_{{prefix_lc}}_zb_endpoint) {
    bool on = val.b;
    esp_zigbee_lock_acquire(portMAX_DELAY);
    (void)ezb_zcl_set_attr_value(s_{{prefix_lc}}_zb_endpoint,
        EZB_ZCL_CLUSTER_ID_ON_OFF, EZB_ZCL_CLUSTER_SERVER,
        EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID, EZB_ZCL_STD_MANUF_CODE, &on, false);
    esp_zigbee_lock_release();
}
