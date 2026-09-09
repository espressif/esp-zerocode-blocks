case {{cfg.value_param}}: {
    if (s_{{prefix_lc}}_val_label != NULL)
        lv_label_set_text_fmt(s_{{prefix_lc}}_val_label, "%d.%d %%", val.i16 / 100, (abs(val.i16) % 100) / 10);
    break;
}
