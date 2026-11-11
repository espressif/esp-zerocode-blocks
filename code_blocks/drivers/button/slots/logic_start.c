{
    /* The press that woke the device, delivered after app_logic_init(): a
     * saved value restored at boot (nvs_persist) would otherwise undo its
     * toggle, and the product's own listeners would not be registered yet. */
    if (s_{{prefix_lc}}_button && {{prefix_lc}}_woke_device() &&
        gpio_get_level((gpio_num_t){{prefix}}_BUTTON_GPIO) != {{prefix}}_BUTTON_ACTIVE_LEVEL) {
        ESP_LOGI(TAG, "Button {{prefix_lc}} woke the device — delivering that press");
{{#if cfg.press_param}}        app_driver_param_val_t {{prefix_lc}}_wv = { .u8 = 1 };
        app_driver_fire_event({{cfg.press_param}}, {{prefix_lc}}_wv, APP_DRIVER_SOURCE_LOCAL);
{{/if}}        {{prefix_lc}}_toggle();
    }
}
