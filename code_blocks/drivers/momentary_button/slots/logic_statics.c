static button_handle_t s_{{prefix_lc}}_button = NULL;

/* Optional override — implement this in your app code to react to {{prefix_lc}} press.
 * Default no-op so the linker is happy when no custom handler is provided. */
__attribute__((weak)) void {{prefix_lc}}_on_press(void)
{
    ESP_LOGI(TAG, "{{prefix_lc}}: button pressed (no handler installed)");
}

static void {{prefix_lc}}_button_cb(void *arg, void *usr_data)
{
    {{prefix_lc}}_on_press();
}
