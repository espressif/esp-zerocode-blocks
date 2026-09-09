case {{cfg.speed_param}}: {
    if (s_{{prefix_lc}}_spd_slider != NULL)
        lv_slider_set_value(s_{{prefix_lc}}_spd_slider, (int32_t)val.u8, LV_ANIM_OFF);
    break;
}
