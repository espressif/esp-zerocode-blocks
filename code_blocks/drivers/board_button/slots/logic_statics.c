/* {{prefix_lc}}: the board's own '{{cfg.device}}' button device.
 * No GPIO number appears anywhere in this product — the pin, active level and
 * timing are the BOARD's facts, and claiming the pin here would be an IO
 * conflict with the board's own peripheral. dev_button is built ON iot_button
 * (its handles ARE button_handle_t), so this is an ordinary iot_button
 * callback on a device somebody else created. */
static void {{prefix_lc}}_board_button_cb(void *arg, void *usr_data)
{
    app_driver_param_val_t val;
    app_driver_get_param({{cfg.target_param}}, &val);
    val.b = !val.b;
    ESP_LOGI(TAG, "{{prefix_lc}}: board button '{{cfg.device}}' pressed — toggling {{cfg.target_param}}");
    app_driver_set_param({{cfg.target_param}}, val, APP_DRIVER_SOURCE_LOCAL);
}
