case {{cfg.dryness_param}}: {
    if (s_{{prefix_lc}}_dry_dd != NULL && val.u8 < 4)
        lv_dropdown_set_selected(s_{{prefix_lc}}_dry_dd, val.u8);
    break;
}
