case {{cfg.position_param}}: {
    zc_espnow_send("{{cfg.position_param}}", 1 /* u8 */, val.u8);
    return;
}
