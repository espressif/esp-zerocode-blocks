case {{cfg.enabled_param}}: {
    if (s_{{prefix_lc}}_pw_sw != NULL) {
        if (val.b) lv_obj_add_state(s_{{prefix_lc}}_pw_sw, LV_STATE_CHECKED);
        else       lv_obj_remove_state(s_{{prefix_lc}}_pw_sw, LV_STATE_CHECKED);
    }
    break;
}
