case {{cfg.value_param}}: {
    /* Already under lvgl_port_lock (taken centrally) — do not lock here. */
    if (s_{{prefix_lc}}_value_label != NULL) {
        lv_label_set_text_fmt(s_{{prefix_lc}}_value_label, "%s%d.%d °C", val.i16 < 0 ? "-" : "", abs(val.i16) / 100, (abs(val.i16) % 100) / 10);
    }
    break;
}
