{
    ezb_zcl_basic_cluster_server_config_t basic_cfg = {
        .zcl_version = EZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,
        .power_source = EZB_ZCL_BASIC_POWER_SOURCE_DEFAULT_VALUE,
    };
    ezb_zcl_identify_cluster_server_config_t identify_cfg = {
        .identify_time = EZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE,
    };
    ezb_zcl_occupancy_sensing_cluster_server_config_t meas_cfg = {
        .occupancy = EZB_ZCL_OCCUPANCY_SENSING_OCCUPANCY_UNOCCUPIED,
        .occupancy_sensor_type = EZB_ZCL_OCCUPANCY_SENSING_OCCUPANCY_SENSOR_TYPE_PIR,
        .occupancy_sensor_type_bitmap = EZB_ZCL_OCCUPANCY_SENSING_OCCUPANCY_SENSOR_TYPE_BITMAP_PIR,
    };
    ezb_af_ep_config_t ep_cfg = {
        .ep_id = {{cfg.zb_endpoint}},
        .app_profile_id = EZB_AF_HA_PROFILE_ID,
        .app_device_id = EZB_ZHA_SIMPLE_SENSOR_DEVICE_ID,
        .app_device_version = 0,
    };
    ezb_af_ep_desc_t ep_desc = ezb_af_create_endpoint_desc(&ep_cfg);
    ESP_ERROR_CHECK(ezb_af_endpoint_add_cluster_desc(ep_desc,
        ezb_zcl_basic_create_cluster_desc(&basic_cfg, EZB_ZCL_CLUSTER_SERVER)));
    ESP_ERROR_CHECK(ezb_af_endpoint_add_cluster_desc(ep_desc,
        ezb_zcl_identify_create_cluster_desc(&identify_cfg, EZB_ZCL_CLUSTER_SERVER)));
    ESP_ERROR_CHECK(ezb_af_endpoint_add_cluster_desc(ep_desc,
        ezb_zcl_occupancy_sensing_create_cluster_desc(&meas_cfg, EZB_ZCL_CLUSTER_SERVER)));
    ESP_ERROR_CHECK(ezb_af_device_add_endpoint_desc(dev_desc, ep_desc));
    s_{{prefix_lc}}_zb_endpoint = {{cfg.zb_endpoint}};
    ESP_LOGI(TAG, "{{prefix_lc}}: Zigbee Occupancy Sensor endpoint %d", {{cfg.zb_endpoint}});
}
