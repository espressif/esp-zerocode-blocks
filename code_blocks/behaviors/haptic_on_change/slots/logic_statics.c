static void {{prefix_lc}}_haptic_cb(
    app_driver_param_id_t param_id, app_driver_param_val_t val,
    app_driver_handle_t source, void *ctx)
{
    (void)val; (void)ctx;
    if (param_id != {{cfg.watch_param}}) return;
    /* LOCAL only — see the block description. A remote change is somebody
       else's action, and buzzing for it makes the panel twitch on its own. */
    if (source != APP_DRIVER_SOURCE_LOCAL) return;
    app_driver_param_val_t buzz = { .u8 = 1 };
    app_driver_set_param({{cfg.trigger_param}}, buzz, APP_DRIVER_SOURCE_LOCAL);
}
