static button_handle_t s_{{prefix_lc}}_fr_btn = NULL;

static void {{prefix_lc}}_factory_reset_cb(void *arg, void *usr_data)
{
    ESP_LOGW(TAG, "{{prefix_lc}}: long-press detected — factory reset");
    app_utils_factory_reset();
}
