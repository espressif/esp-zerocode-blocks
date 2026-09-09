static button_handle_t s_{{prefix_lc}}_btn = NULL;

__attribute__((weak)) void {{prefix_lc}}_on_double_click(void)
{
    ESP_LOGI(TAG, "{{prefix_lc}}: double-click (no handler installed)");
}
__attribute__((weak)) void {{prefix_lc}}_on_long_press(void)
{
    ESP_LOGI(TAG, "{{prefix_lc}}: long-press (no handler installed)");
}

static void {{prefix_lc}}_single_click_cb(void *arg, void *usr_data)
{
    app_driver_param_val_t v;
    app_driver_get_param({{cfg.target_param}}, &v);
    v.b = !v.b;
    ESP_LOGI(TAG, "{{prefix_lc}}: single-click — toggling {{cfg.target_param}}");
    app_driver_set_param({{cfg.target_param}}, v, APP_DRIVER_SOURCE_LOCAL);
}
static void {{prefix_lc}}_double_click_cb(void *arg, void *usr_data)  { {{prefix_lc}}_on_double_click(); }
static void {{prefix_lc}}_long_press_cb (void *arg, void *usr_data)  { {{prefix_lc}}_on_long_press();  }
