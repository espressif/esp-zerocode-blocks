case {{cfg.occupied_param}}: {
    if (s_{{prefix_lc}}_st_label != NULL)
        lv_label_set_text(s_{{prefix_lc}}_st_label, (val.b) ? "Occupied" : "Clear");
    break;
}
