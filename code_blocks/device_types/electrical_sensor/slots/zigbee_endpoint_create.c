{
    ezb_zcl_basic_cluster_server_config_t basic_cfg = {
        .zcl_version = EZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,
        .power_source = EZB_ZCL_BASIC_POWER_SOURCE_DEFAULT_VALUE,
    };
    ezb_zcl_identify_cluster_server_config_t identify_cfg = {
        .identify_time = EZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE,
    };
    ezb_zcl_electrical_measurement_cluster_server_config_t meas_cfg = {
        .measurement_type =
            EZB_ZCL_ELECTRICAL_MEASUREMENT_MEASUREMENT_TYPE_ACTIVE_MEASUREMENT_AC |
            EZB_ZCL_ELECTRICAL_MEASUREMENT_MEASUREMENT_TYPE_PHASE_A_MEASUREMENT,
    };
    ezb_zcl_cluster_desc_t meas_desc =
        ezb_zcl_electrical_measurement_create_cluster_desc(&meas_cfg, EZB_ZCL_CLUSTER_SERVER);
    uint16_t {{prefix_lc}}_one = 1;
    uint16_t {{prefix_lc}}_div10 = 10;
    int16_t {{prefix_lc}}_power_init = 0;
    ESP_ERROR_CHECK(ezb_zcl_electrical_measurement_cluster_desc_add_attr(meas_desc,
        EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_ID, &{{prefix_lc}}_power_init));
    ESP_ERROR_CHECK(ezb_zcl_electrical_measurement_cluster_desc_add_attr(meas_desc,
        EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_POWER_MULTIPLIER_ID, &{{prefix_lc}}_one));
    ESP_ERROR_CHECK(ezb_zcl_electrical_measurement_cluster_desc_add_attr(meas_desc,
        EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_POWER_DIVISOR_ID, &{{prefix_lc}}_div10));
    {{#if cfg.voltage_param}}
    uint16_t {{prefix_lc}}_volt_init = 0;
    ESP_ERROR_CHECK(ezb_zcl_electrical_measurement_cluster_desc_add_attr(meas_desc,
        EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_ID, &{{prefix_lc}}_volt_init));
    ESP_ERROR_CHECK(ezb_zcl_electrical_measurement_cluster_desc_add_attr(meas_desc,
        EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_VOLTAGE_MULTIPLIER_ID, &{{prefix_lc}}_one));
    ESP_ERROR_CHECK(ezb_zcl_electrical_measurement_cluster_desc_add_attr(meas_desc,
        EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_VOLTAGE_DIVISOR_ID, &{{prefix_lc}}_div10));
    {{/if}}
    {{#if cfg.current_param}}
    uint16_t {{prefix_lc}}_curr_init = 0;
    uint16_t {{prefix_lc}}_div1000 = 1000;
    ESP_ERROR_CHECK(ezb_zcl_electrical_measurement_cluster_desc_add_attr(meas_desc,
        EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_ID, &{{prefix_lc}}_curr_init));
    ESP_ERROR_CHECK(ezb_zcl_electrical_measurement_cluster_desc_add_attr(meas_desc,
        EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_CURRENT_MULTIPLIER_ID, &{{prefix_lc}}_one));
    ESP_ERROR_CHECK(ezb_zcl_electrical_measurement_cluster_desc_add_attr(meas_desc,
        EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_CURRENT_DIVISOR_ID, &{{prefix_lc}}_div1000));
    {{/if}}
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
    ESP_ERROR_CHECK(ezb_af_endpoint_add_cluster_desc(ep_desc, meas_desc));
    ESP_ERROR_CHECK(ezb_af_device_add_endpoint_desc(dev_desc, ep_desc));
    s_{{prefix_lc}}_zb_endpoint = {{cfg.zb_endpoint}};
    ESP_LOGI(TAG, "{{prefix_lc}}: Zigbee Electrical Sensor endpoint %d", {{cfg.zb_endpoint}});
}
