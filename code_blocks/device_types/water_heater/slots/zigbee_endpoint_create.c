{
    app_driver_param_val_t init = {};
    ezb_zcl_on_off_cluster_server_config_t {{prefix_lc}}_on_off_cfg = {};
    app_driver_get_param({{cfg.power_param}}, &init);
    {{prefix_lc}}_on_off_cfg.on_off = init.b;
    ezb_zha_thermostat_config_t {{prefix_lc}}_cfg = EZB_ZHA_THERMOSTAT_CONFIG();
    app_driver_get_param({{cfg.setpoint_param}}, &init);
    int16_t {{prefix_lc}}_heat_setpoint = init.i16;
    /* Water heater heats — mirrors the Matter heating feature flag. */
    {{prefix_lc}}_cfg.thermostat_cfg.control_sequence_of_operation =
        EZB_ZCL_THERMOSTAT_CONTROL_SEQUENCE_OF_OPERATION_HEATING_ONLY;
    {{prefix_lc}}_cfg.thermostat_cfg.system_mode = EZB_ZCL_THERMOSTAT_SYSTEM_MODE_HEAT;
    ezb_af_ep_desc_t ep_desc = ezb_zha_create_thermostat({{cfg.zb_endpoint}}, &{{prefix_lc}}_cfg);
    if (ep_desc == EZB_INVALID_AF_EP_DESC) return ESP_ERR_NO_MEM;
    ezb_zcl_cluster_desc_t thermostat_desc =
        ezb_af_endpoint_get_cluster_desc(ep_desc, EZB_ZCL_CLUSTER_ID_THERMOSTAT, EZB_ZCL_CLUSTER_SERVER);
    ESP_ERROR_CHECK(esp_zigbee_err_to_esp(ezb_zcl_thermostat_cluster_desc_add_attr(
        thermostat_desc, EZB_ZCL_ATTR_THERMOSTAT_OCCUPIED_HEATING_SETPOINT_ID, &{{prefix_lc}}_heat_setpoint)));
    ESP_ERROR_CHECK(esp_zigbee_err_to_esp(ezb_af_endpoint_add_cluster_desc(
        ep_desc, ezb_zcl_on_off_create_cluster_desc(&{{prefix_lc}}_on_off_cfg, EZB_ZCL_CLUSTER_SERVER))));
    ESP_ERROR_CHECK(esp_zigbee_err_to_esp(ezb_af_device_add_endpoint_desc(dev_desc, ep_desc)));
    s_{{prefix_lc}}_zb_endpoint = {{cfg.zb_endpoint}};
    ESP_LOGI(TAG, "{{prefix_lc}}: Zigbee Water Heater (Thermostat) endpoint %d", {{cfg.zb_endpoint}});
}
