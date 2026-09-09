case {{cfg.value_param}}: {
    if (s_{{prefix_lc}}_val_label != NULL)
        lv_label_set_text_fmt(s_{{prefix_lc}}_val_label, "%u", (unsigned)val.u16);
    break;
}
