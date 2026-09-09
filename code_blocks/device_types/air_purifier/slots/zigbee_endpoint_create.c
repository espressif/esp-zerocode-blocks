{
    ezb_af_ep_config_t {{prefix_lc}}_ep_cfg = {
        .ep_id = {{cfg.zb_endpoint}},
        .app_profile_id = EZB_AF_HA_PROFILE_ID,
        .app_device_id = EZB_ZHA_HEATING_COOLING_UNIT_DEVICE_ID,
        .app_device_version = 0,
    };
    ezb_af_ep_desc_t ep_desc = ezb_af_create_endpoint_desc(&{{prefix_lc}}_ep_cfg);
    if (ep_desc == EZB_INVALID_AF_EP_DESC) return ESP_ERR_NO_MEM;
    ezb_zha_common_device_config_t {{prefix_lc}}_common_cfg = EZB_ZHA_COMMON_DEVICE_CONFIG();
    ezb_zcl_fan_control_cluster_server_config_t {{prefix_lc}}_fan_cfg = {};
    app_driver_param_val_t init = {};
    app_driver_get_param({{cfg.speed_param}}, &init);
    uint8_t init_pct = init.u8 > 100 ? 100 : init.u8;
    {{prefix_lc}}_fan_cfg.fan_mode =
        init_pct == 0  ? EZB_ZCL_FAN_CONTROL_FAN_MODE_OFF :
        init_pct <= 33 ? EZB_ZCL_FAN_CONTROL_FAN_MODE_LOW :
        init_pct <= 66 ? EZB_ZCL_FAN_CONTROL_FAN_MODE_MEDIUM :
                         EZB_ZCL_FAN_CONTROL_FAN_MODE_HIGH;
    {{prefix_lc}}_fan_cfg.fan_mode_sequence = EZB_ZCL_FAN_CONTROL_FAN_MODE_SEQUENCE_LOW_MED_HIGH;
    ESP_ERROR_CHECK(esp_zigbee_err_to_esp(ezb_af_endpoint_add_cluster_desc(
        ep_desc, ezb_zcl_basic_create_cluster_desc(&{{prefix_lc}}_common_cfg.basic_cfg, EZB_ZCL_CLUSTER_SERVER))));
    ESP_ERROR_CHECK(esp_zigbee_err_to_esp(ezb_af_endpoint_add_cluster_desc(
        ep_desc, ezb_zcl_identify_create_cluster_desc(&{{prefix_lc}}_common_cfg.identify_cfg, EZB_ZCL_CLUSTER_SERVER))));
    ESP_ERROR_CHECK(esp_zigbee_err_to_esp(ezb_af_endpoint_add_cluster_desc(
        ep_desc, ezb_zcl_fan_control_create_cluster_desc(&{{prefix_lc}}_fan_cfg, EZB_ZCL_CLUSTER_SERVER))));
    ESP_ERROR_CHECK(esp_zigbee_err_to_esp(ezb_af_device_add_endpoint_desc(dev_desc, ep_desc)));
    s_{{prefix_lc}}_zb_endpoint = {{cfg.zb_endpoint}};
    ESP_LOGI(TAG, "{{prefix_lc}}: Zigbee Air Purifier (Heating/Cooling Unit) endpoint %d", {{cfg.zb_endpoint}});
}
