case {{cfg.run_mode_param}}: {
    if (s_{{prefix_lc}}_mode_dd != NULL && val.u8 < 3)
        lv_dropdown_set_selected(s_{{prefix_lc}}_mode_dd, val.u8);
    break;
}
