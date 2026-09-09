if (param_id == {{cfg.setpoint_param}} && s_{{prefix_lc}}_zb_endpoint) {
    s_{{prefix_lc}}_setpoint_val = val.i16;
    ezb_zcl_attribute_t attr = {};
    attr.id = EZB_ZCL_ATTR_THERMOSTAT_OCCUPIED_HEATING_SETPOINT_ID;
    attr.data.type = EZB_ZCL_ATTR_TYPE_INT16;
    attr.data.size = sizeof(int16_t);
    attr.data.value = &s_{{prefix_lc}}_setpoint_val;
    ezb_zcl_write_attr_cmd_t cmd = {};
    cmd.cmd_ctrl.dst_addr.addr_mode = EZB_ADDR_MODE_NONE;
    cmd.cmd_ctrl.src_ep = s_{{prefix_lc}}_zb_endpoint;
    cmd.cmd_ctrl.cluster_id = EZB_ZCL_CLUSTER_ID_THERMOSTAT;
    cmd.cmd_ctrl.manuf_code = EZB_ZCL_STD_MANUF_CODE;
    cmd.payload.attr_number = 1;
    cmd.payload.attr_field = &attr;
    if (esp_zigbee_lock_acquire(portMAX_DELAY)) {
        (void)ezb_zcl_write_attr_cmd_req(&cmd);
        esp_zigbee_lock_release();
    }
    ESP_LOGI(TAG, "{{prefix_lc}}: wrote OccupiedHeatingSetpoint %d to bound targets", val.i16);
}
