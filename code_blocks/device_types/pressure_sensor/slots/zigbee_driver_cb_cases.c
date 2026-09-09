if (param_id == {{cfg.value_param}} && s_{{prefix_lc}}_zb_endpoint) {
    int16_t v = val.i16;
    if (esp_zigbee_lock_acquire(portMAX_DELAY)) {
        (void)ezb_zcl_set_attr_value(s_{{prefix_lc}}_zb_endpoint,
            EZB_ZCL_CLUSTER_ID_PRESSURE_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER,
            EZB_ZCL_ATTR_PRESSURE_MEASUREMENT_MEASURED_VALUE_ID,
            EZB_ZCL_STD_MANUF_CODE, &v, false);
        esp_zigbee_lock_release();
    }
}
