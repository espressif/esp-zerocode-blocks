case {{cfg.power_param}}: {
    if (s_{{prefix_lc}}_pw_label != NULL)
        lv_label_set_text_fmt(s_{{prefix_lc}}_pw_label, "%d W", (int)val.i16);
    break;
}
case {{cfg.energy_param}}: {
    if (s_{{prefix_lc}}_en_label != NULL)
        lv_label_set_text_fmt(s_{{prefix_lc}}_en_label, "%u Wh", (unsigned)val.u32);
    break;
}
