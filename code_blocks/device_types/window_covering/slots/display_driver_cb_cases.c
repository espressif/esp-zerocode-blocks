case {{cfg.position_param}}: {
    if (s_{{prefix_lc}}_pos_slider != NULL)
        lv_slider_set_value(s_{{prefix_lc}}_pos_slider, (int32_t)(val.u16 / 100), LV_ANIM_OFF);
    break;
}
