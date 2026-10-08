/* {{prefix_lc}}: the board's own '{{cfg.device}}' button device.
 * No GPIO number appears anywhere in this product — the pin and active level
 * are the BOARD's facts, and claiming the pin here would be an IO conflict
 * with the board's own peripheral. dev_button is built ON iot_button (its
 * handles ARE button_handle_t), so these are ordinary iot_button callbacks on
 * a device somebody else created.
 *
 * Gesture hooks: weak, so the product overrides any of them with a plain
 * definition of the same signature in its own file. They run on the button
 * timer task — keep them short and non-blocking. */
{{#if cfg.target_param}}#define {{prefix}}_SINGLE_CLICK_TOGGLES 1
{{/if}}__attribute__((weak)) void {{prefix_lc}}_on_single_click(void)
{
#ifndef {{prefix}}_SINGLE_CLICK_TOGGLES
    ESP_LOGI(TAG, "{{prefix_lc}}: single click (no handler)");
#endif
    /* With target_param bound the click already toggled it — nothing more. */
}
__attribute__((weak)) void {{prefix_lc}}_on_double_click(void)
{
    ESP_LOGI(TAG, "{{prefix_lc}}: double click (no handler)");
}
__attribute__((weak)) void {{prefix_lc}}_on_long_press(void)
{
    ESP_LOGI(TAG, "{{prefix_lc}}: long press (no handler)");
}

static void {{prefix_lc}}_board_button_cb(void *arg, void *usr_data)
{
    (void)arg; (void)usr_data;
{{#if cfg.target_param}}    app_driver_param_val_t val;
    app_driver_get_param({{cfg.target_param}}, &val);
    val.b = !val.b;
    ESP_LOGI(TAG, "{{prefix_lc}}: board button '{{cfg.device}}' pressed — toggling {{cfg.target_param}}");
    app_driver_set_param({{cfg.target_param}}, val, APP_DRIVER_SOURCE_LOCAL);
{{/if}}    {{prefix_lc}}_on_single_click();
}
static void {{prefix_lc}}_double_click_cb(void *arg, void *usr_data) { (void)arg; (void)usr_data; {{prefix_lc}}_on_double_click(); }
static void {{prefix_lc}}_long_press_cb(void *arg, void *usr_data)   { (void)arg; (void)usr_data; {{prefix_lc}}_on_long_press(); }
{{#if cfg.factory_reset_ms}}
static void {{prefix_lc}}_factory_reset_cb(void *arg, void *usr_data)
{
    (void)arg; (void)usr_data;
    ESP_LOGW(TAG, "{{prefix_lc}}: held {{cfg.factory_reset_ms}} ms — erasing NVS and rebooting");
    nvs_flash_erase();
    esp_restart();
}
{{/if}}
