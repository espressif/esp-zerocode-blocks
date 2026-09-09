{
    ezb_zcl_basic_cluster_server_config_t basic_cfg = {
        .zcl_version = EZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,
        .power_source = EZB_ZCL_BASIC_POWER_SOURCE_DEFAULT_VALUE,
    };
    ezb_zcl_identify_cluster_server_config_t identify_cfg = {
        .identify_time = EZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE,
    };
    app_driver_param_val_t init = {};
    app_driver_get_param({{cfg.mode_param}}, &init);
    ezb_zcl_multistate_value_cluster_server_config_t mv_cfg = {
        .number_of_states = {{cfg.mode_count}},
        .out_of_service = false,
        .present_value = init.u8,
        .status_flags = EZB_ZCL_MULTISTATE_VALUE_STATUS_FLAGS_DEFAULT_VALUE,
    };
    ezb_af_ep_config_t ep_cfg = {
        .ep_id = {{cfg.zb_endpoint}},
        .app_profile_id = EZB_AF_HA_PROFILE_ID,
        .app_device_id = 0xfff2, /* Custom Attributes Device */
        .app_device_version = 0,
    };
    ezb_af_ep_desc_t ep_desc = ezb_af_create_endpoint_desc(&ep_cfg);
    ESP_ERROR_CHECK(ezb_af_endpoint_add_cluster_desc(ep_desc,
        ezb_zcl_basic_create_cluster_desc(&basic_cfg, EZB_ZCL_CLUSTER_SERVER)));
    ESP_ERROR_CHECK(ezb_af_endpoint_add_cluster_desc(ep_desc,
        ezb_zcl_identify_create_cluster_desc(&identify_cfg, EZB_ZCL_CLUSTER_SERVER)));
    ESP_ERROR_CHECK(ezb_af_endpoint_add_cluster_desc(ep_desc,
        ezb_zcl_multistate_value_create_cluster_desc(&mv_cfg, EZB_ZCL_CLUSTER_SERVER)));
    ESP_ERROR_CHECK(ezb_af_device_add_endpoint_desc(dev_desc, ep_desc));
    s_{{prefix_lc}}_zb_endpoint = {{cfg.zb_endpoint}};
    ESP_LOGI(TAG, "{{prefix_lc}}: Zigbee Multistate Value (mode) endpoint %d", {{cfg.zb_endpoint}});
}
