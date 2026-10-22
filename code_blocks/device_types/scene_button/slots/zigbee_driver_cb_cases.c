/* A controller turns a param into a COMMAND to bound devices, so the shell's
 * post-join/reboot resync (ZB_SOURCE_RESYNC) must not reach it: replaying the
 * remembered state would switch every bound device after each reboot. */
/* A position of 0 carries no press (the position_param contract), as in the
 * Matter binding. */
if (param_id == {{cfg.position_param}} && val.u8 != 0 && s_{{prefix_lc}}_zb_endpoint && source != ZB_SOURCE_RESYNC) {
    ezb_zcl_on_off_cmd_t cmd = {};
    cmd.cmd_ctrl.dst_addr.addr_mode = EZB_ADDR_MODE_NONE;
    cmd.cmd_ctrl.src_ep = s_{{prefix_lc}}_zb_endpoint;
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
