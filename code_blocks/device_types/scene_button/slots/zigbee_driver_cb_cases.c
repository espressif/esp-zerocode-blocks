if (param_id == {{cfg.position_param}} && s_{{prefix_lc}}_zb_endpoint) {
    ezb_zcl_on_off_cmd_t cmd = {
        .cmd_ctrl = {
            .dst_addr = { .addr_mode = EZB_ADDR_MODE_NONE },
            .src_ep = s_{{prefix_lc}}_zb_endpoint,
        },
    };
    if (esp_zigbee_lock_acquire(portMAX_DELAY)) {
        ezb_err_t err = ezb_zcl_on_off_toggle_cmd_req(&cmd);
        esp_zigbee_lock_release();
        if (err == EZB_ERR_NONE) {
            ESP_LOGI(TAG, "{{prefix_lc}}: sent Toggle to bound targets");
        } else {
            ESP_LOGW(TAG, "{{prefix_lc}}: failed to send Toggle (0x%x)", err);
        }
    } else {
        ESP_LOGW(TAG, "{{prefix_lc}}: failed to acquire Zigbee lock for Toggle command");
    }
}
