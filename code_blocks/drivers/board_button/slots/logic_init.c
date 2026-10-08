{
    /* esp_board_manager_init() ran in app_main BEFORE app_logic_init(), so
       the iot_button device for '{{cfg.device}}' already exists — this asks
       for its handle and registers callbacks; it creates NOTHING. Going
       through app_button_get_or_create() here would make a SECOND iot_button
       device on a pin the board's device already owns — the exact race the
       registry exists to prevent. */
    void *{{prefix_lc}}_h = NULL;
    if (esp_board_manager_get_device_handle("{{cfg.device}}", &{{prefix_lc}}_h) != ESP_OK || {{prefix_lc}}_h == NULL) {
        /* Generation refuses a device the board does not declare, so reaching
           here means the board manager failed to bring it up at boot.
           Degraded, not fatal: the product keeps running without this input. */
        ESP_LOGE(TAG, "{{prefix_lc}}: board device '{{cfg.device}}' has no handle — button disabled");
    } else {
        dev_button_handles_t *{{prefix_lc}}_btns = (dev_button_handles_t *){{prefix_lc}}_h;
        if ({{prefix_lc}}_btns->num_buttons < 1 || {{prefix_lc}}_btns->button_handles[0] == NULL) {
            ESP_LOGE(TAG, "{{prefix_lc}}: board device '{{cfg.device}}' carries no button handle — button disabled");
        } else {
            button_handle_t {{prefix_lc}}_btn = {{prefix_lc}}_btns->button_handles[0];
            bool {{prefix_lc}}_bound = true;
{{#if cfg.double_click_ms}}            /* Device-wide: the double-click window, and with it how long a single
               click waits to be reported. Overrides the board's own timing. */
            iot_button_set_param({{prefix_lc}}_btn, BUTTON_SHORT_PRESS_TIME_MS, (void *)(intptr_t){{cfg.double_click_ms}});
{{/if}}            if (iot_button_register_cb({{prefix_lc}}_btn, BUTTON_SINGLE_CLICK, NULL, {{prefix_lc}}_board_button_cb, NULL) != ESP_OK) {
                ESP_LOGE(TAG, "{{prefix_lc}}: single click not bound — check the button timings (a hold must be longer than the double-click window)");
                {{prefix_lc}}_bound = false;
            }
            if (iot_button_register_cb({{prefix_lc}}_btn, BUTTON_DOUBLE_CLICK, NULL, {{prefix_lc}}_double_click_cb, NULL) != ESP_OK) {
                ESP_LOGE(TAG, "{{prefix_lc}}: double click not bound — check the button timings (a hold must be longer than the double-click window)");
                {{prefix_lc}}_bound = false;
            }
            /* Long-press thresholds are PER CALLBACK (event_args), so the hook
               and the factory reset each keep their own hold time. */
            button_event_args_t {{prefix_lc}}_lp = {};
            {{prefix_lc}}_lp.long_press.press_time = {{cfg.long_press_ms}};
            if (iot_button_register_cb({{prefix_lc}}_btn, BUTTON_LONG_PRESS_START, &{{prefix_lc}}_lp, {{prefix_lc}}_long_press_cb, NULL) != ESP_OK) {
                ESP_LOGE(TAG, "{{prefix_lc}}: long press not bound — check the button timings (a hold must be longer than the double-click window)");
                {{prefix_lc}}_bound = false;
            }
{{#if cfg.factory_reset_ms}}            button_event_args_t {{prefix_lc}}_fr = {};
            {{prefix_lc}}_fr.long_press.press_time = {{cfg.factory_reset_ms}};
            if (iot_button_register_cb({{prefix_lc}}_btn, BUTTON_LONG_PRESS_START, &{{prefix_lc}}_fr, {{prefix_lc}}_factory_reset_cb, NULL) != ESP_OK) {
                ESP_LOGE(TAG, "{{prefix_lc}}: factory-reset hold not bound — check the button timings (a hold must be longer than the double-click window)");
                {{prefix_lc}}_bound = false;
            } else {
                ESP_LOGI(TAG, "{{prefix_lc}}: factory reset armed — hold '{{cfg.device}}' for {{cfg.factory_reset_ms}} ms");
            }
{{/if}}            if ({{prefix_lc}}_bound) {
                ESP_LOGI(TAG, "{{prefix_lc}}: board button '{{cfg.device}}' bound (click / double click / long press {{cfg.long_press_ms}} ms)");
            }
        }
    }
}
