if ((param_id == {{cfg.smoke_state_param}} || param_id == {{cfg.co_state_param}}) &&
    s_{{prefix_lc}}_zb_endpoint) {
    /* Tri-state (0=Normal, 1=Warning, 2=Critical): nonzero → bit set.
     * smoke → ALARM1, CO → ALARM2 (see binding comment above). */
    if (param_id == {{cfg.smoke_state_param}}) {
        if (val.u8) s_{{prefix_lc}}_zb_status |= EZB_ZCL_IAS_ZONE_ZONE_STATUS_ALARM1;
        else s_{{prefix_lc}}_zb_status &= (uint16_t)~EZB_ZCL_IAS_ZONE_ZONE_STATUS_ALARM1;
    } else {
        if (val.u8) s_{{prefix_lc}}_zb_status |= EZB_ZCL_IAS_ZONE_ZONE_STATUS_ALARM2;
        else s_{{prefix_lc}}_zb_status &= (uint16_t)~EZB_ZCL_IAS_ZONE_ZONE_STATUS_ALARM2;
    }
    uint16_t status = s_{{prefix_lc}}_zb_status;
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
