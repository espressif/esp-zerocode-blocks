if (param_id == {{cfg.leak_param}} && s_{{prefix_lc}}_zb_endpoint) {
    /* Driver: true = leak detected; IAS ALARM1 = alarmed. */
    uint16_t status = val.b ? EZB_ZCL_IAS_ZONE_ZONE_STATUS_ALARM1 : 0;
    if (esp_zigbee_lock_acquire(portMAX_DELAY)) {
        /* SDK 2.0 sends Zone Status Change Notification when this
         * ZoneStatus attribute changes; there is no separate public send API. */
        (void)ezb_zcl_set_attr_value(s_{{prefix_lc}}_zb_endpoint,
            EZB_ZCL_CLUSTER_ID_IAS_ZONE, EZB_ZCL_CLUSTER_SERVER,
            EZB_ZCL_ATTR_IAS_ZONE_ZONE_STATUS_ID, EZB_ZCL_STD_MANUF_CODE,
            &status, false);
        esp_zigbee_lock_release();
    }
}
