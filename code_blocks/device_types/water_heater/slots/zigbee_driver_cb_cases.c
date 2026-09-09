if (param_id == {{cfg.power_param}} && s_{{prefix_lc}}_zb_endpoint) {
    bool on = val.b;
    if (esp_zigbee_lock_acquire(portMAX_DELAY)) {
        ezb_zcl_set_attr_value(s_{{prefix_lc}}_zb_endpoint,
            EZB_ZCL_CLUSTER_ID_ON_OFF, EZB_ZCL_CLUSTER_SERVER,
            EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID, EZB_ZCL_STD_MANUF_CODE, &on, false);
        esp_zigbee_lock_release();
    }
}
if (param_id == {{cfg.setpoint_param}} && s_{{prefix_lc}}_zb_endpoint) {
    int16_t v = val.i16;
    if (esp_zigbee_lock_acquire(portMAX_DELAY)) {
        ezb_zcl_set_attr_value(s_{{prefix_lc}}_zb_endpoint,
            EZB_ZCL_CLUSTER_ID_THERMOSTAT, EZB_ZCL_CLUSTER_SERVER,
            EZB_ZCL_ATTR_THERMOSTAT_OCCUPIED_HEATING_SETPOINT_ID, EZB_ZCL_STD_MANUF_CODE, &v, false);
        esp_zigbee_lock_release();
    }
}
