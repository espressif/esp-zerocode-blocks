{
    ezb_zcl_basic_cluster_server_config_t basic_cfg = {
        .zcl_version = EZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,
        .power_source = EZB_ZCL_BASIC_POWER_SOURCE_DEFAULT_VALUE,
    };
    ezb_zcl_identify_cluster_server_config_t identify_cfg = {
        .identify_time = EZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE,
    };
    ezb_zcl_electrical_measurement_cluster_server_config_t em_cfg = {
        .measurement_type =
            EZB_ZCL_ELECTRICAL_MEASUREMENT_MEASUREMENT_TYPE_ACTIVE_MEASUREMENT_AC |
            EZB_ZCL_ELECTRICAL_MEASUREMENT_MEASUREMENT_TYPE_PHASE_A_MEASUREMENT,
    };
    ezb_zcl_cluster_desc_t em_desc =
        ezb_zcl_electrical_measurement_create_cluster_desc(&em_cfg, EZB_ZCL_CLUSTER_SERVER);
    app_driver_param_val_t init = {};
    app_driver_get_param({{cfg.power_param}}, &init);
    /* Driver power is signed watts (positive = charging); ActivePower is i16 W. */
    int16_t {{prefix_lc}}_power_w = init.i16;
    ESP_ERROR_CHECK(ezb_zcl_electrical_measurement_cluster_desc_add_attr(em_desc,
        EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_ID, &{{prefix_lc}}_power_w));
    ezb_af_ep_config_t ep_cfg = {
        .ep_id = {{cfg.zb_endpoint}},
        .app_profile_id = EZB_AF_HA_PROFILE_ID,
        .app_device_id = EZB_ZHA_METER_INTERFACE_DEVICE_ID,
        .app_device_version = 0,
    };
    ezb_af_ep_desc_t ep_desc = ezb_af_create_endpoint_desc(&ep_cfg);
    ESP_ERROR_CHECK(ezb_af_endpoint_add_cluster_desc(ep_desc,
        ezb_zcl_basic_create_cluster_desc(&basic_cfg, EZB_ZCL_CLUSTER_SERVER)));
    ESP_ERROR_CHECK(ezb_af_endpoint_add_cluster_desc(ep_desc,
        ezb_zcl_identify_create_cluster_desc(&identify_cfg, EZB_ZCL_CLUSTER_SERVER)));
    ESP_ERROR_CHECK(ezb_af_endpoint_add_cluster_desc(ep_desc, em_desc));
    ESP_ERROR_CHECK(ezb_af_device_add_endpoint_desc(dev_desc, ep_desc));
    s_{{prefix_lc}}_zb_endpoint = {{cfg.zb_endpoint}};
    ESP_LOGI(TAG, "{{prefix_lc}}: Zigbee Electrical Measurement (battery) endpoint %d", {{cfg.zb_endpoint}});
}
