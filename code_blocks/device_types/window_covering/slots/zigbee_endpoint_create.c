{
    ezb_zha_window_covering_config_t {{prefix_lc}}_cfg = EZB_ZHA_WINDOW_COVERING_CONFIG();
    {{prefix_lc}}_cfg.window_covering_cfg.config_status =
        EZB_ZCL_WINDOW_COVERING_CONFIG_STATUS_OPERATIONAL |
        EZB_ZCL_WINDOW_COVERING_CONFIG_STATUS_ONLINE;
    {{prefix_lc}}_cfg.window_covering_cfg.mode =
        EZB_ZCL_WINDOW_COVERING_MODE_DEFAULT_VALUE;
    ezb_af_ep_desc_t ep_desc =
        ezb_zha_create_window_covering({{cfg.zb_endpoint}}, &{{prefix_lc}}_cfg);
    if (ep_desc == EZB_INVALID_AF_EP_DESC) return ESP_ERR_NO_MEM;
    ezb_zcl_cluster_desc_t wc_desc =
        ezb_af_endpoint_get_cluster_desc(ep_desc, EZB_ZCL_CLUSTER_ID_WINDOW_COVERING, EZB_ZCL_CLUSTER_SERVER);
    app_driver_param_val_t init = {};
    app_driver_get_param({{cfg.position_param}}, &init);
    /* Driver position is centi-percent (0-10000); ZCL lift percentage is u8 0-100. */
    uint8_t {{prefix_lc}}_lift_pct = (uint8_t)(init.u16 / 100 > 100 ? 100 : init.u16 / 100);
    ESP_ERROR_CHECK(esp_zigbee_err_to_esp(ezb_zcl_window_covering_cluster_desc_add_attr(
        wc_desc, EZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_LIFT_PERCENTAGE_ID,
        &{{prefix_lc}}_lift_pct)));
    ESP_ERROR_CHECK(esp_zigbee_err_to_esp(ezb_af_device_add_endpoint_desc(dev_desc, ep_desc)));
    s_{{prefix_lc}}_zb_endpoint = {{cfg.zb_endpoint}};
    ESP_LOGI(TAG, "{{prefix_lc}}: Zigbee Window Covering endpoint %d", {{cfg.zb_endpoint}});
}
