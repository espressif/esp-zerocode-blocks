{
    ezb_zha_color_dimmable_light_config_t {{prefix_lc}}_cfg =
        EZB_ZHA_COLOR_DIMMABLE_LIGHT_CONFIG();
    app_driver_param_val_t init = {};
    app_driver_get_param({{cfg.power_param}}, &init);
    {{prefix_lc}}_cfg.on_off_cfg.on_off = init.b;
    app_driver_get_param({{cfg.brightness_param}}, &init);
    {{prefix_lc}}_cfg.level_cfg.current_level = init.u8;
    {{prefix_lc}}_cfg.color_cfg.color_mode =
        EZB_ZCL_COLOR_CONTROL_COLOR_MODE_CURRENT_HUE_AND_CURRENT_SATURATION;
    {{prefix_lc}}_cfg.color_cfg.enhanced_color_mode =
        EZB_ZCL_COLOR_CONTROL_ENHANCED_COLOR_MODE_CURRENT_HUE_AND_CURRENT_SATURATION;
    {{prefix_lc}}_cfg.color_cfg.color_capabilities =
        EZB_ZCL_COLOR_CONTROL_COLOR_CAPABILITIES_HUE_SATURATION_SUPPORTED;

    ezb_af_ep_desc_t ep_desc =
        ezb_zha_create_color_dimmable_light({{cfg.zb_endpoint}}, &{{prefix_lc}}_cfg);
    if (ep_desc == EZB_INVALID_AF_EP_DESC) {
        ESP_LOGE(TAG, "{{prefix_lc}}: failed to create Zigbee Color Dimmable Light endpoint");
        return ESP_ERR_NO_MEM;
    }
    ezb_zcl_cluster_desc_t color_desc = ezb_af_endpoint_get_cluster_desc(
        ep_desc, EZB_ZCL_CLUSTER_ID_COLOR_CONTROL, EZB_ZCL_CLUSTER_SERVER);
    if (color_desc == EZB_INVALID_ZCL_CLUSTER_DESC) {
        ESP_LOGE(TAG, "{{prefix_lc}}: Zigbee Color Control cluster is missing");
        return ESP_ERR_INVALID_STATE;
    }
    app_driver_get_param({{cfg.hue_param}}, &init);
    uint8_t {{prefix_lc}}_hue = init.u8;
    ezb_err_t err = ezb_zcl_color_control_cluster_desc_add_attr(
        color_desc, EZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_HUE_ID, &{{prefix_lc}}_hue);
    app_driver_get_param({{cfg.saturation_param}}, &init);
    uint8_t {{prefix_lc}}_sat = init.u8;
    if (err == EZB_ERR_NONE) {
        err = ezb_zcl_color_control_cluster_desc_add_attr(
            color_desc, EZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_SATURATION_ID, &{{prefix_lc}}_sat);
    }
    if (err == EZB_ERR_NONE) {
        err = ezb_af_device_add_endpoint_desc(dev_desc, ep_desc);
    }
    if (err != EZB_ERR_NONE) {
        ESP_LOGE(TAG, "{{prefix_lc}}: failed to configure Zigbee endpoint (0x%x)", err);
        return esp_zigbee_err_to_esp(err);
    }
    s_{{prefix_lc}}_zb_endpoint = {{cfg.zb_endpoint}};
    ESP_LOGI(TAG, "{{prefix_lc}}: Zigbee Color Dimmable Light endpoint %d", {{cfg.zb_endpoint}});
}
