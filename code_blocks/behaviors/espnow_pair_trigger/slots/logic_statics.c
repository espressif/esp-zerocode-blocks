static app_driver_handle_t s_{{prefix_lc}}_pair_handle = 0;

static void {{prefix_lc}}_pair_cb(app_driver_param_id_t id, app_driver_param_val_t val,
                                  app_driver_handle_t source, void *ctx)
{
    (void)source; (void)ctx;
    if (id != {{cfg.trigger_param}} || !val.b) return;
    zc_espnow_pair_start((uint32_t){{prefix}}_PAIR_WINDOW_S * 1000UL);
}
