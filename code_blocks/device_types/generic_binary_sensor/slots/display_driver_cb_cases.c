case {{cfg.value_param}}: {
    if (s_{{prefix_lc}}_st_label != NULL)
        lv_label_set_text(s_{{prefix_lc}}_st_label, val.b ? "{{cfg.on_text}}" : "{{cfg.off_text}}");
    break;
}
