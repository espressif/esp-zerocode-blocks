case {{cfg.local_temp_param}}: {
    if (s_{{prefix_lc}}_cur_label != NULL)
        lv_label_set_text_fmt(s_{{prefix_lc}}_cur_label, "%s%d.%d °C", val.i16 < 0 ? "-" : "", abs(val.i16) / 100, (abs(val.i16) % 100) / 10);
    break;
}
case {{cfg.heat_setpoint_param}}: {
    if (s_{{prefix_lc}}_sp_slider != NULL)
        lv_slider_set_value(s_{{prefix_lc}}_sp_slider, (int32_t)(val.i16 / 100), LV_ANIM_OFF);
    break;
}
