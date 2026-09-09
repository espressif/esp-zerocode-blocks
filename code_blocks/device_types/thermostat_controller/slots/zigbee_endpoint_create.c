{
    ezb_af_ep_config_t {{prefix_lc}}_ep_cfg = {
        .ep_id = {{cfg.zb_endpoint}},
        .app_profile_id = EZB_AF_HA_PROFILE_ID,
        .app_device_id = EZB_ZHA_COMBINED_INTERFACE_DEVICE_ID,
        .app_device_version = 0,
    };
    ezb_af_ep_desc_t ep_desc = ezb_af_create_endpoint_desc(&{{prefix_lc}}_ep_cfg);
    if (ep_desc == EZB_INVALID_AF_EP_DESC) return ESP_ERR_NO_MEM;
    ezb_zha_common_device_config_t {{prefix_lc}}_cfg = EZB_ZHA_COMMON_DEVICE_CONFIG();
    ESP_ERROR_CHECK(esp_zigbee_err_to_esp(ezb_af_endpoint_add_cluster_desc(
        ep_desc, ezb_zcl_basic_create_cluster_desc(&{{prefix_lc}}_cfg.basic_cfg, EZB_ZCL_CLUSTER_SERVER))));
    ESP_ERROR_CHECK(esp_zigbee_err_to_esp(ezb_af_endpoint_add_cluster_desc(
        ep_desc, ezb_zcl_identify_create_cluster_desc(&{{prefix_lc}}_cfg.identify_cfg, EZB_ZCL_CLUSTER_SERVER))));
    ESP_ERROR_CHECK(esp_zigbee_err_to_esp(ezb_af_endpoint_add_cluster_desc(
        ep_desc, ezb_zcl_thermostat_create_cluster_desc(NULL, EZB_ZCL_CLUSTER_CLIENT))));
    ESP_ERROR_CHECK(esp_zigbee_err_to_esp(ezb_af_device_add_endpoint_desc(dev_desc, ep_desc)));
    s_{{prefix_lc}}_zb_endpoint = {{cfg.zb_endpoint}};
    ESP_LOGI(TAG, "{{prefix_lc}}: Zigbee Thermostat Controller (client) endpoint %d", {{cfg.zb_endpoint}});
}
