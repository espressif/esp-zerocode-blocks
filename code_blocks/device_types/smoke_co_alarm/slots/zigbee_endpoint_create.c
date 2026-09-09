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
        .zone_type = EZB_ZCL_IAS_ZONE_ZONE_TYPE_FIRE_SENSOR,
        .ias_cie_address = 0,
        .zone_id = EZB_ZCL_IAS_ZONE_ZONE_ID_DEFAULT_VALUE,
    };
    app_driver_param_val_t init = {};
    /* smoke → ALARM1, CO → ALARM2; nonzero tri-state = bit set. */
    app_driver_get_param({{cfg.smoke_state_param}}, &init);
    if (init.u8) s_{{prefix_lc}}_zb_status |= EZB_ZCL_IAS_ZONE_ZONE_STATUS_ALARM1;
    app_driver_get_param({{cfg.co_state_param}}, &init);
    if (init.u8) s_{{prefix_lc}}_zb_status |= EZB_ZCL_IAS_ZONE_ZONE_STATUS_ALARM2;
    zone_cfg.zone_status = s_{{prefix_lc}}_zb_status;

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
    ESP_LOGI(TAG, "{{prefix_lc}}: Zigbee IAS Zone (fire sensor) endpoint %d", {{cfg.zb_endpoint}});
}
