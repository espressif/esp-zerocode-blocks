{
    /* esp_board_manager_init() ran in app_main BEFORE app_logic_init(), so
       the iot_button device for '{{cfg.device}}' already exists — this asks
       for its handle and registers a callback; it creates NOTHING. Going
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
            iot_button_register_cb({{prefix_lc}}_btns->button_handles[0], BUTTON_SINGLE_CLICK, NULL,
                                   {{prefix_lc}}_board_button_cb, NULL);
            ESP_LOGI(TAG, "{{prefix_lc}}: board button '{{cfg.device}}' bound — single click toggles {{cfg.target_param}}");
        }
    }
}
