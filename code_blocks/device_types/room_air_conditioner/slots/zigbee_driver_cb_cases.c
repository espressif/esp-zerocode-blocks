if (param_id == {{cfg.power_param}} && s_{{prefix_lc}}_zb_endpoint) {
    bool on = val.b;
    if (esp_zigbee_lock_acquire(portMAX_DELAY)) {
        ezb_zcl_set_attr_value(s_{{prefix_lc}}_zb_endpoint,
            EZB_ZCL_CLUSTER_ID_ON_OFF, EZB_ZCL_CLUSTER_SERVER,
            EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID, EZB_ZCL_STD_MANUF_CODE, &on, false);
        esp_zigbee_lock_release();
    }
}
if (param_id == {{cfg.cool_setpoint_param}} && s_{{prefix_lc}}_zb_endpoint) {
    int16_t v = val.i16;
    if (esp_zigbee_lock_acquire(portMAX_DELAY)) {
        ezb_zcl_set_attr_value(s_{{prefix_lc}}_zb_endpoint,
            EZB_ZCL_CLUSTER_ID_THERMOSTAT, EZB_ZCL_CLUSTER_SERVER,
            EZB_ZCL_ATTR_THERMOSTAT_OCCUPIED_COOLING_SETPOINT_ID, EZB_ZCL_STD_MANUF_CODE, &v, false);
        esp_zigbee_lock_release();
    }
}
if (param_id == {{cfg.fan_speed_param}} && s_{{prefix_lc}}_zb_endpoint) {
    /* percent → FanMode bucketing: 0=off, 1-33=low, 34-66=medium, 67-100=high */
    uint8_t pct = val.u8 > 100 ? 100 : val.u8;
    uint8_t mode = pct == 0  ? EZB_ZCL_FAN_CONTROL_FAN_MODE_OFF
                 : pct <= 33 ? EZB_ZCL_FAN_CONTROL_FAN_MODE_LOW
                 : pct <= 66 ? EZB_ZCL_FAN_CONTROL_FAN_MODE_MEDIUM
                             : EZB_ZCL_FAN_CONTROL_FAN_MODE_HIGH;
    if (esp_zigbee_lock_acquire(portMAX_DELAY)) {
        ezb_zcl_set_attr_value(s_{{prefix_lc}}_zb_endpoint,
            EZB_ZCL_CLUSTER_ID_FAN_CONTROL, EZB_ZCL_CLUSTER_SERVER,
            EZB_ZCL_ATTR_FAN_CONTROL_FAN_MODE_ID, EZB_ZCL_STD_MANUF_CODE, &mode, false);
        esp_zigbee_lock_release();
    }
}
