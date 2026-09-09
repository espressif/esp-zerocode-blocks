if (param_id == {{cfg.power_param}} && s_{{prefix_lc}}_zb_endpoint) {
    bool on = val.b;
    if (esp_zigbee_lock_acquire(portMAX_DELAY)) {
        (void)ezb_zcl_set_attr_value(s_{{prefix_lc}}_zb_endpoint,
            EZB_ZCL_CLUSTER_ID_ON_OFF, EZB_ZCL_CLUSTER_SERVER,
            EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID, EZB_ZCL_STD_MANUF_CODE, &on, false);
        esp_zigbee_lock_release();
    }
}
if (param_id == {{cfg.brightness_param}} && s_{{prefix_lc}}_zb_endpoint) {
    uint8_t lvl = val.u8;   /* driver + ZCL CurrentLevel both 0-254 */
    if (esp_zigbee_lock_acquire(portMAX_DELAY)) {
        (void)ezb_zcl_set_attr_value(s_{{prefix_lc}}_zb_endpoint,
            EZB_ZCL_CLUSTER_ID_LEVEL, EZB_ZCL_CLUSTER_SERVER,
            EZB_ZCL_ATTR_LEVEL_CURRENT_LEVEL_ID, EZB_ZCL_STD_MANUF_CODE, &lvl, false);
        esp_zigbee_lock_release();
    }
}
if (param_id == {{cfg.color_temp_param}} && s_{{prefix_lc}}_zb_endpoint) {
    uint16_t mireds = val.u16;   /* driver + ZCL ColorTemperatureMireds both mireds */
    if (esp_zigbee_lock_acquire(portMAX_DELAY)) {
        (void)ezb_zcl_set_attr_value(s_{{prefix_lc}}_zb_endpoint,
            EZB_ZCL_CLUSTER_ID_COLOR_CONTROL, EZB_ZCL_CLUSTER_SERVER,
            EZB_ZCL_ATTR_COLOR_CONTROL_COLOR_TEMPERATURE_MIREDS_ID,
            EZB_ZCL_STD_MANUF_CODE, &mireds, false);
        esp_zigbee_lock_release();
    }
}
