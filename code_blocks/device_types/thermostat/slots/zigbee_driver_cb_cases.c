if (param_id == {{cfg.local_temp_param}} && s_{{prefix_lc}}_zb_endpoint) {
    int16_t v = val.i16;
    esp_zigbee_lock_acquire(portMAX_DELAY);
    (void)ezb_zcl_set_attr_value(s_{{prefix_lc}}_zb_endpoint,
        EZB_ZCL_CLUSTER_ID_THERMOSTAT, EZB_ZCL_CLUSTER_SERVER,
        EZB_ZCL_ATTR_THERMOSTAT_LOCAL_TEMPERATURE_ID, EZB_ZCL_STD_MANUF_CODE, &v, false);
    esp_zigbee_lock_release();
}
if (param_id == {{cfg.heat_setpoint_param}} && s_{{prefix_lc}}_zb_endpoint) {
    int16_t v = val.i16;
    esp_zigbee_lock_acquire(portMAX_DELAY);
    (void)ezb_zcl_set_attr_value(s_{{prefix_lc}}_zb_endpoint,
        EZB_ZCL_CLUSTER_ID_THERMOSTAT, EZB_ZCL_CLUSTER_SERVER,
        EZB_ZCL_ATTR_THERMOSTAT_OCCUPIED_HEATING_SETPOINT_ID, EZB_ZCL_STD_MANUF_CODE, &v, false);
    esp_zigbee_lock_release();
}
if (param_id == {{cfg.system_mode_param}} && s_{{prefix_lc}}_zb_endpoint) {
    uint8_t mode = val.u8;
    esp_zigbee_lock_acquire(portMAX_DELAY);
    (void)ezb_zcl_set_attr_value(s_{{prefix_lc}}_zb_endpoint,
        EZB_ZCL_CLUSTER_ID_THERMOSTAT, EZB_ZCL_CLUSTER_SERVER,
        EZB_ZCL_ATTR_THERMOSTAT_SYSTEM_MODE_ID, EZB_ZCL_STD_MANUF_CODE, &mode, false);
    esp_zigbee_lock_release();
}
