{
    ezb_zha_mains_power_outlet_config_t {{prefix_lc}}_cfg = EZB_ZHA_MAINS_POWER_OUTLET_CONFIG();
    app_driver_param_val_t init = {};
    app_driver_get_param({{cfg.enabled_param}}, &init);
    {{prefix_lc}}_cfg.on_off_cfg.on_off = init.b;
    ezb_af_ep_desc_t ep_desc =
        ezb_zha_create_mains_power_outlet({{cfg.zb_endpoint}}, &{{prefix_lc}}_cfg);
    if (ep_desc == EZB_INVALID_AF_EP_DESC) {
        ESP_LOGE(TAG, "{{prefix_lc}}: failed to create Zigbee Mains Power Outlet (EVSE) endpoint");
        return ESP_ERR_NO_MEM;
    }
    ezb_err_t err = ezb_af_device_add_endpoint_desc(dev_desc, ep_desc);
    if (err != EZB_ERR_NONE) {
        ESP_LOGE(TAG, "{{prefix_lc}}: failed to add Zigbee endpoint (0x%x)", err);
        return esp_zigbee_err_to_esp(err);
    }
    s_{{prefix_lc}}_zb_endpoint = {{cfg.zb_endpoint}};
    ESP_LOGI(TAG, "{{prefix_lc}}: Zigbee Mains Power Outlet (EVSE) endpoint %d", {{cfg.zb_endpoint}});
}
