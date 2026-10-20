/* A change made AT the device: a physical button (APP_DRIVER_SOURCE_LOCAL) or a
   touch on the device's own screen. The display framework writes with its own
   solution handle, not LOCAL (it needs that to skip its own echo), so it is
   recognised by name — handles are assigned in registration order and differ
   per product. */
static bool {{prefix_lc}}_is_at_device(app_driver_handle_t source)
{
    if (source == APP_DRIVER_SOURCE_LOCAL) return true;
    const char *name = app_driver_solution_name(source);
    return name != NULL && strcmp(name, "display") == 0;
}

static void {{prefix_lc}}_haptic_cb(
    app_driver_param_id_t param_id, app_driver_param_val_t val,
    app_driver_handle_t source, void *ctx)
{
    (void)val; (void)ctx;
    if (param_id != {{cfg.watch_param}}) return;
    /* At-the-device only — see the block description. A remote change is
       somebody else's action, and buzzing for it makes the panel twitch on its
       own. */
    if (!{{prefix_lc}}_is_at_device(source)) return;
    /* An EVENT, not state: every press must buzz. set_param would store 1 on
       the first press and dedupe every later one (nothing clears the trigger),
       so the motor would buzz once per boot. */
    app_driver_param_val_t buzz = { .u8 = 1 };
    app_driver_fire_event({{cfg.trigger_param}}, buzz, APP_DRIVER_SOURCE_LOCAL);
}
