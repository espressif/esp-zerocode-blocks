{
    /* The output is off from boot: write that onto the param as state, so it
     * is known from the start rather than "never written" until something
     * first switches it. A value already written — restored by an earlier
     * block — is left alone, and a later restore (nvs_persist_bool) still
     * switches it on. */
    if (!app_driver_param_seen({{cfg.param_id}})) {
        app_driver_param_val_t {{prefix_lc}}_boot = {};
        {{prefix_lc}}_boot.b = false;
        app_driver_set_param({{cfg.param_id}}, {{prefix_lc}}_boot, APP_DRIVER_SOURCE_RESTORE);
    }
}
