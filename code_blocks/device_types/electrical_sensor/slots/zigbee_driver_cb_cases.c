if (param_id == {{cfg.active_power_param}} && s_{{prefix_lc}}_zb_endpoint) {
    /* Driver carries mW; ActivePower is i16 with ACPowerDivisor=10 (deciwatts). */
    uint32_t dw = val.u32 / 100U;
    int16_t v = dw > 32767U ? (int16_t)32767 : (int16_t)dw;
    if (esp_zigbee_lock_acquire(portMAX_DELAY)) {
        (void)ezb_zcl_set_attr_value(s_{{prefix_lc}}_zb_endpoint,
            EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER,
            EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_ID,
            EZB_ZCL_STD_MANUF_CODE, &v, false);
        esp_zigbee_lock_release();
    }
}
{{#if cfg.voltage_param}}
if (param_id == {{cfg.voltage_param}} && s_{{prefix_lc}}_zb_endpoint) {
    /* Driver carries mV; RMSVoltage is u16 with ACVoltageDivisor=10 (decivolts). */
    uint32_t dv = val.u32 / 100U;
    uint16_t v = dv > 0xFFFEU ? (uint16_t)0xFFFEU : (uint16_t)dv;
    if (esp_zigbee_lock_acquire(portMAX_DELAY)) {
        (void)ezb_zcl_set_attr_value(s_{{prefix_lc}}_zb_endpoint,
            EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER,
            EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_ID,
            EZB_ZCL_STD_MANUF_CODE, &v, false);
        esp_zigbee_lock_release();
    }
}
{{/if}}
{{#if cfg.current_param}}
if (param_id == {{cfg.current_param}} && s_{{prefix_lc}}_zb_endpoint) {
    /* Driver carries mA; RMSCurrent is u16 with ACCurrentDivisor=1000 (mA as-is). */
    uint32_t ma = val.u32;
    uint16_t v = ma > 0xFFFEU ? (uint16_t)0xFFFEU : (uint16_t)ma;
    if (esp_zigbee_lock_acquire(portMAX_DELAY)) {
        (void)ezb_zcl_set_attr_value(s_{{prefix_lc}}_zb_endpoint,
            EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER,
            EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_ID,
            EZB_ZCL_STD_MANUF_CODE, &v, false);
        esp_zigbee_lock_release();
    }
}
{{/if}}
