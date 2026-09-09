{
    ezb_zha_on_off_switch_config_t {{prefix_lc}}_cfg = EZB_ZHA_ON_OFF_SWITCH_CONFIG();
    ezb_af_ep_desc_t ep_desc =
        ezb_zha_create_on_off_switch({{cfg.zb_endpoint}}, &{{prefix_lc}}_cfg);
    if (ep_desc == EZB_INVALID_AF_EP_DESC) {
        ESP_LOGE(TAG, "{{prefix_lc}}: failed to create Zigbee Pump Controller (On/Off Switch client) endpoint");
        return ESP_ERR_NO_MEM;
    }
    ezb_err_t err = ezb_af_device_add_endpoint_desc(dev_desc, ep_desc);
    if (err != EZB_ERR_NONE) {
        ESP_LOGE(TAG, "{{prefix_lc}}: failed to add Zigbee endpoint (0x%x)", err);
        return esp_zigbee_err_to_esp(err);
    }
    s_{{prefix_lc}}_zb_endpoint = {{cfg.zb_endpoint}};
    ESP_LOGI(TAG, "{{prefix_lc}}: Zigbee Pump Controller (On/Off Switch client) endpoint %d", {{cfg.zb_endpoint}});
}
