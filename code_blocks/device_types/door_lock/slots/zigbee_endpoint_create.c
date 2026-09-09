{
    ezb_zha_door_lock_config_t {{prefix_lc}}_cfg = EZB_ZHA_DOOR_LOCK_CONFIG();
    app_driver_param_val_t init = {};
    app_driver_get_param({{cfg.locked_param}}, &init);
    {{prefix_lc}}_cfg.door_lock_cfg.lock_state = init.b ? EZB_ZCL_DOOR_LOCK_LOCK_STATE_LOCKED : EZB_ZCL_DOOR_LOCK_LOCK_STATE_UNLOCKED;
    ezb_af_ep_desc_t ep_desc =
        ezb_zha_create_door_lock({{cfg.zb_endpoint}}, &{{prefix_lc}}_cfg);
    if (ep_desc == EZB_INVALID_AF_EP_DESC) {
        ESP_LOGE(TAG, "{{prefix_lc}}: failed to create Zigbee Door Lock endpoint");
        return ESP_ERR_NO_MEM;
    }
    ezb_err_t err = ezb_af_device_add_endpoint_desc(dev_desc, ep_desc);
    if (err != EZB_ERR_NONE) {
        ESP_LOGE(TAG, "{{prefix_lc}}: failed to add Zigbee endpoint (0x%x)", err);
        return esp_zigbee_err_to_esp(err);
    }
    s_{{prefix_lc}}_zb_endpoint = {{cfg.zb_endpoint}};
    ESP_LOGI(TAG, "{{prefix_lc}}: Zigbee Door Lock endpoint %d (state reporting only)", {{cfg.zb_endpoint}});
}
