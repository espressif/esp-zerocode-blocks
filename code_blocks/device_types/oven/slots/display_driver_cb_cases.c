case {{cfg.setpoint_param}}: {
    if (s_{{prefix_lc}}_sp_slider != NULL)
        lv_slider_set_value(s_{{prefix_lc}}_sp_slider, (int32_t)(val.i16 / 100), LV_ANIM_OFF);
    break;
}
