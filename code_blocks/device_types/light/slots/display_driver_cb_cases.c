case {{cfg.power_param}}: {
    /* Already under lvgl_port_lock (taken centrally) — do not lock here. */
    if (s_{{prefix_lc}}_switch != NULL) {
        if (val.b) lv_obj_add_state(s_{{prefix_lc}}_switch, LV_STATE_CHECKED);
        else       lv_obj_remove_state(s_{{prefix_lc}}_switch, LV_STATE_CHECKED);
    }
    break;
}
