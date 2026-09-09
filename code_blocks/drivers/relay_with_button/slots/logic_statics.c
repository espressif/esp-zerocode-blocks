static button_handle_t s_{{prefix_lc}}_btn = NULL;

static void {{prefix_lc}}_btn_cb(void *arg, void *usr_data)
{
    app_driver_param_val_t v;
    app_driver_get_param({{cfg.param_id}}, &v);
    v.b = !v.b;
    app_driver_set_param({{cfg.param_id}}, v, APP_DRIVER_SOURCE_LOCAL);
}
