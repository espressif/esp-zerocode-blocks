case {{cfg.smoke_state_param}}: {
    if (s_{{prefix_lc}}_smoke_label != NULL)
        lv_label_set_text(s_{{prefix_lc}}_smoke_label, (val.u8 != 0) ? "SMOKE!" : "Smoke OK");
    break;
}
case {{cfg.co_state_param}}: {
    if (s_{{prefix_lc}}_co_label != NULL)
        lv_label_set_text(s_{{prefix_lc}}_co_label, (val.u8 != 0) ? "CO!" : "CO OK");
    break;
}
