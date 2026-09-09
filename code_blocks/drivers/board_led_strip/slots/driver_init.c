{
    /* esp_board_manager_init() ran in app_main BEFORE app_driver_init(), so
       the led_strip for '{{cfg.device}}' already exists — this asks for the
       handle, it does not create anything. The config comes along for the LED
       count: how many pixels the board soldered on is the board's fact. */
    void *{{prefix_lc}}_h = NULL;
    dev_led_strip_config_t *{{prefix_lc}}_cfg = NULL;
    if (esp_board_manager_get_device_handle("{{cfg.device}}", &{{prefix_lc}}_h) != ESP_OK ||
        esp_board_manager_get_device_config("{{cfg.device}}", (void **)&{{prefix_lc}}_cfg) != ESP_OK ||
        {{prefix_lc}}_h == NULL || {{prefix_lc}}_cfg == NULL) {
        /* Generation refuses a device the board does not declare, so reaching
           here means the board manager failed to bring it up at boot. Degraded,
           not fatal: the product keeps running without this indicator. */
        ESP_LOGE(TAG, "{{prefix_lc}}: board device '{{cfg.device}}' has no handle — indicator disabled");
    } else {
        s_{{prefix_lc}}_strip = ((dev_led_strip_handles_t *){{prefix_lc}}_h)->strip_handle;
        s_{{prefix_lc}}_leds  = {{prefix_lc}}_cfg->strip_config.max_leds;
        if (s_{{prefix_lc}}_leds == 0) {
            s_{{prefix_lc}}_leds = 1;
        }
        /* BORN OFF — whatever the strip showed at board init, deassert before
           any param can arrive. Same rule as "actuators must be born safe". */
        {{prefix_lc}}_board_strip_set(false);
        /* Observe, don't own: notified on every {{cfg.param_id}} change from
           any other source (button, console, a transport). */
        app_driver_register_solution("{{prefix_lc}}_board_led", {{prefix_lc}}_board_strip_notify, NULL);
        ESP_LOGI(TAG, "{{prefix_lc}}: board led_strip '{{cfg.device}}' bound (%u LED(s), shows {{cfg.param_id}})",
                 (unsigned)s_{{prefix_lc}}_leds);
    }
}
