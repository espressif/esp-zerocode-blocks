case {{cfg.position_param}}: {
    /* A boot-time restore is local state, not a command to the peers. */
    if (source == APP_DRIVER_SOURCE_RESTORE) return;
    zc_espnow_send("{{cfg.position_param}}", 1 /* u8 */, val.u8);
    return;
}
