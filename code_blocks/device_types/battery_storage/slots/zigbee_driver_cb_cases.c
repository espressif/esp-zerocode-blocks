if (param_id == {{cfg.power_param}} && s_{{prefix_lc}}_zb_endpoint) {
    /* Driver power is signed watts (positive = charging); ActivePower is i16 W. */
    int16_t power_w = val.i16;
    if (esp_zigbee_lock_acquire(portMAX_DELAY)) {
        (void)ezb_zcl_set_attr_value(s_{{prefix_lc}}_zb_endpoint,
            EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER,
            EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_ID,
            EZB_ZCL_STD_MANUF_CODE, &power_w, false);
        esp_zigbee_lock_release();
    }
}
/* {{prefix_lc}}: energy_param not bound to Zigbee (see block.yml note). */
