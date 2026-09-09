case {{cfg.cook_time_param}}: {
    if (s_{{prefix_lc}}_time_slider != NULL)
        lv_slider_set_value(s_{{prefix_lc}}_time_slider, (int32_t)val.u32, LV_ANIM_OFF);
    break;
}
case {{cfg.power_level_param}}: {
    if (s_{{prefix_lc}}_pwr_slider != NULL)
        lv_slider_set_value(s_{{prefix_lc}}_pwr_slider, (int32_t)val.u8, LV_ANIM_OFF);
    break;
}
