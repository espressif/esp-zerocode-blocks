if (param_id == {{cfg.mode_param}} && s_{{prefix_lc}}_zb_endpoint) {
    /* Driver mode index is u8; ZCL Multistate PresentValue is u16. */
    uint16_t mode = val.u8;
    if (esp_zigbee_lock_acquire(portMAX_DELAY)) {
        (void)ezb_zcl_set_attr_value(s_{{prefix_lc}}_zb_endpoint,
            EZB_ZCL_CLUSTER_ID_MULTISTATE_VALUE, EZB_ZCL_CLUSTER_SERVER,
            EZB_ZCL_ATTR_MULTISTATE_VALUE_PRESENT_VALUE_ID,
            EZB_ZCL_STD_MANUF_CODE, &mode, false);
        esp_zigbee_lock_release();
    }
}
