{
    /* esp_board_manager_init() ran in app_main BEFORE app_driver_init(), so
       the handle for '{{cfg.device}}' already exists — this asks for it, it
       does not create anything. The config comes along for active_level:
       whether the board's LED is lit by a 1 or a 0 is the board's fact, not
       this product's. */
    void *{{prefix_lc}}_h = NULL;
    dev_gpio_ctrl_config_t *{{prefix_lc}}_cfg = NULL;
    if (esp_board_manager_get_device_handle("{{cfg.device}}", &{{prefix_lc}}_h) != ESP_OK ||
        esp_board_manager_get_device_config("{{cfg.device}}", (void **)&{{prefix_lc}}_cfg) != ESP_OK ||
        {{prefix_lc}}_h == NULL || {{prefix_lc}}_cfg == NULL) {
        /* Generation refuses a device the board does not declare, so reaching
           here means the board manager failed to bring it up at boot. Degraded,
           not fatal: the product keeps running without this output. */
        ESP_LOGE(TAG, "{{prefix_lc}}: board device '{{cfg.device}}' has no handle — output disabled");
    } else {
        s_{{prefix_lc}}_gpio   = (int)((periph_gpio_handle_t *){{prefix_lc}}_h)->gpio_num;
        s_{{prefix_lc}}_active = (int){{prefix_lc}}_cfg->active_level;
        /* BORN OFF — and this is not belt-and-braces. dev_gpio_ctrl_init()
           drives the line to active_level at board init, whatever
           default_level says, so the device is ON by the time we get here. On
           an LED that is a boot flash; on the relay or motor the same adapter
           can drive it is a pulse to the load. Deassert first, before any
           param can arrive. Same rule as "actuators must be born safe". */
        {{prefix_lc}}_board_gpio_set(false);
        ESP_LOGI(TAG, "{{prefix_lc}}: board device '{{cfg.device}}' on GPIO %d (active level %d)",
                 s_{{prefix_lc}}_gpio, s_{{prefix_lc}}_active);
    }
}
