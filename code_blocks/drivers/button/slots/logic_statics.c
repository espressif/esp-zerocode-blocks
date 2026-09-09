static button_handle_t s_{{prefix_lc}}_button = NULL;

static void {{prefix_lc}}_button_cb(void *arg, void *usr_data)
{
    app_driver_param_val_t val;
    app_driver_get_param({{cfg.target_param}}, &val);
    val.b = !val.b;
    ESP_LOGI(TAG, "Button {{prefix_lc}} pressed — toggling {{cfg.target_param}}");
    app_driver_set_param({{cfg.target_param}}, val, APP_DRIVER_SOURCE_LOCAL);
}
