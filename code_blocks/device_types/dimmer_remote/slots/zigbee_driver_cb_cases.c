if (param_id == {{cfg.power_param}} && s_{{prefix_lc}}_zb_endpoint) {
    ezb_zcl_on_off_cmd_t cmd = {
        .cmd_ctrl = {
            .dst_addr = { .addr_mode = EZB_ADDR_MODE_NONE },
            .src_ep = s_{{prefix_lc}}_zb_endpoint,
        },
    };
    if (esp_zigbee_lock_acquire(portMAX_DELAY)) {
        ezb_err_t err = val.b ? ezb_zcl_on_off_on_cmd_req(&cmd)
                              : ezb_zcl_on_off_off_cmd_req(&cmd);
        esp_zigbee_lock_release();
        if (err == EZB_ERR_NONE) {
            ESP_LOGI(TAG, "{{prefix_lc}}: sent %s to bound targets", val.b ? "On" : "Off");
        } else {
            ESP_LOGW(TAG, "{{prefix_lc}}: failed to send %s (0x%x)", val.b ? "On" : "Off", err);
        }
    } else {
        ESP_LOGW(TAG, "{{prefix_lc}}: failed to acquire Zigbee lock for On/Off command");
    }
}
if (param_id == {{cfg.level_param}} && s_{{prefix_lc}}_zb_endpoint) {
    ezb_zcl_level_move_to_level_cmd_t cmd = {
        .cmd_ctrl = {
            .dst_addr = { .addr_mode = EZB_ADDR_MODE_NONE },
            .src_ep = s_{{prefix_lc}}_zb_endpoint,
        },
        .payload = {
            .level = val.u8,
            .transition_time = 5, /* 0.5 s */
        },
    };
    if (esp_zigbee_lock_acquire(portMAX_DELAY)) {
        ezb_err_t err = ezb_zcl_level_move_to_level_cmd_req(&cmd);
        esp_zigbee_lock_release();
        if (err == EZB_ERR_NONE) {
            ESP_LOGI(TAG, "{{prefix_lc}}: sent MoveToLevel %d to bound targets", val.u8);
        } else {
            ESP_LOGW(TAG, "{{prefix_lc}}: failed to send MoveToLevel %d (0x%x)", val.u8, err);
        }
    } else {
        ESP_LOGW(TAG, "{{prefix_lc}}: failed to acquire Zigbee lock for MoveToLevel command");
    }
}
