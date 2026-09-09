case {{cfg.power_param}}: {
    if (s_{{prefix_lc}}_pw_sw != NULL) {
        if (val.b) lv_obj_add_state(s_{{prefix_lc}}_pw_sw, LV_STATE_CHECKED);
        else       lv_obj_remove_state(s_{{prefix_lc}}_pw_sw, LV_STATE_CHECKED);
    }
    break;
}
case {{cfg.cool_setpoint_param}}: {
    if (s_{{prefix_lc}}_sp_slider != NULL)
        lv_slider_set_value(s_{{prefix_lc}}_sp_slider, (int32_t)(val.i16 / 100), LV_ANIM_OFF);
    break;
}
