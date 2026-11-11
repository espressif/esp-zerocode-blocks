{
    /* The lights' reported on/off, mirrored onto the remote's param. */
    app_driver_param_val_t {{prefix_lc}}_st = {};
    {{prefix_lc}}_st.b = on;
    app_driver_set_param({{cfg.power_param}}, {{prefix_lc}}_st, s_handle);
}
