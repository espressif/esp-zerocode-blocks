if (param_id == {{cfg.occupied_param}} && s_{{prefix_lc}}_zb_endpoint) {
    uint8_t v = val.b ? 1 : 0; /* bit 0 = occupied */
    if (esp_zigbee_lock_acquire(portMAX_DELAY)) {
        (void)ezb_zcl_set_attr_value(s_{{prefix_lc}}_zb_endpoint,
            EZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING, EZB_ZCL_CLUSTER_SERVER,
            EZB_ZCL_ATTR_OCCUPANCY_SENSING_OCCUPANCY_ID,
            EZB_ZCL_STD_MANUF_CODE, &v, false);
        esp_zigbee_lock_release();
    }
}
