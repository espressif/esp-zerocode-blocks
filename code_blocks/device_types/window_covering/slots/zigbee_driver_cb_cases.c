if (param_id == {{cfg.position_param}} && s_{{prefix_lc}}_zb_endpoint) {
    /* Driver position is centi-percent (0-10000); ZCL lift percentage is u8 0-100. */
    uint16_t centi = val.u16;
    uint8_t pct = (uint8_t)(centi / 100 > 100 ? 100 : centi / 100);
    if (esp_zigbee_lock_acquire(portMAX_DELAY)) {
        ezb_zcl_set_attr_value(s_{{prefix_lc}}_zb_endpoint,
            EZB_ZCL_CLUSTER_ID_WINDOW_COVERING, EZB_ZCL_CLUSTER_SERVER,
            EZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_LIFT_PERCENTAGE_ID,
            EZB_ZCL_STD_MANUF_CODE, &pct, false);
        esp_zigbee_lock_release();
    }
}
