case {{cfg.power_param}}: {
    if (s_{{prefix_lc}}_pw_label != NULL)
        lv_label_set_text_fmt(s_{{prefix_lc}}_pw_label, "%u.%u W", (unsigned)(val.u32 / 1000), (unsigned)((val.u32 % 1000) / 100));
    break;
}
case {{cfg.energy_param}}: {
    if (s_{{prefix_lc}}_en_label != NULL)
        lv_label_set_text_fmt(s_{{prefix_lc}}_en_label, "%u Wh", (unsigned)val.u32);
    break;
}
