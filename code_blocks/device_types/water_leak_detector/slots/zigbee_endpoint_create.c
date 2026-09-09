{
    ezb_zcl_basic_cluster_server_config_t basic_cfg = {
        .zcl_version = EZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,
        .power_source = EZB_ZCL_BASIC_POWER_SOURCE_DEFAULT_VALUE,
    };
    ezb_zcl_identify_cluster_server_config_t identify_cfg = {
        .identify_time = EZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE,
    };
    ezb_zcl_ias_zone_cluster_server_config_t zone_cfg = {
        .zone_state = EZB_ZCL_IAS_ZONE_ZONE_STATE_NOT_ENROLLED,
        .zone_type = EZB_ZCL_IAS_ZONE_ZONE_TYPE_WATER_SENSOR,
        .ias_cie_address = 0,
        .zone_id = EZB_ZCL_IAS_ZONE_ZONE_ID_DEFAULT_VALUE,
    };
    app_driver_param_val_t init = {};
    app_driver_get_param({{cfg.leak_param}}, &init);
    /* Driver convention: true = leak detected. ALARM1 set = alarmed. */
    zone_cfg.zone_status = init.b ? EZB_ZCL_IAS_ZONE_ZONE_STATUS_ALARM1 : 0;

    ezb_af_ep_config_t ep_cfg = {
        .ep_id = {{cfg.zb_endpoint}},
        .app_profile_id = EZB_AF_HA_PROFILE_ID,
        .app_device_id = EZB_ZHA_IAS_ZONE_ID,
        .app_device_version = 0,
    };
    ezb_af_ep_desc_t ep_desc = ezb_af_create_endpoint_desc(&ep_cfg);
    if (ep_desc == EZB_INVALID_AF_EP_DESC) {
        ESP_LOGE(TAG, "{{prefix_lc}}: failed to create Zigbee IAS Zone endpoint");
        return ESP_ERR_NO_MEM;
    }
    ESP_ERROR_CHECK(ezb_af_endpoint_add_cluster_desc(ep_desc,
        ezb_zcl_basic_create_cluster_desc(&basic_cfg, EZB_ZCL_CLUSTER_SERVER)));
    ESP_ERROR_CHECK(ezb_af_endpoint_add_cluster_desc(ep_desc,
        ezb_zcl_identify_create_cluster_desc(&identify_cfg, EZB_ZCL_CLUSTER_SERVER)));
    ESP_ERROR_CHECK(ezb_af_endpoint_add_cluster_desc(ep_desc,
        ezb_zcl_ias_zone_create_cluster_desc(&zone_cfg, EZB_ZCL_CLUSTER_SERVER)));
    ESP_ERROR_CHECK(ezb_af_device_add_endpoint_desc(dev_desc, ep_desc));
    s_{{prefix_lc}}_zb_endpoint = {{cfg.zb_endpoint}};
    ESP_LOGI(TAG, "{{prefix_lc}}: Zigbee IAS Zone (water sensor) endpoint %d", {{cfg.zb_endpoint}});
}
