{
    /* The lights' reported level, on the remote's 0-254 scale. */
    app_driver_param_val_t {{prefix_lc}}_st = {};
    {{prefix_lc}}_st.u8 = zc_ble_mesh_level_to_u8(level);
    app_driver_set_param({{cfg.level_param}}, {{prefix_lc}}_st, s_handle);
}
