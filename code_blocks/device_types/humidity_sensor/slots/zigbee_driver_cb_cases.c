if (param_id == {{cfg.value_param}} && s_{{prefix_lc}}_zb_endpoint) {
    uint16_t v = (uint16_t)val.i16;
    if (esp_zigbee_lock_acquire(portMAX_DELAY)) {
        (void)ezb_zcl_set_attr_value(s_{{prefix_lc}}_zb_endpoint,
            EZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER,
            EZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_MEASURED_VALUE_ID,
            EZB_ZCL_STD_MANUF_CODE, &v, false);
        esp_zigbee_lock_release();
    }
}
