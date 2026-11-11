case {{cfg.power_param}}: {
    /* A boot-time restore is local state, not a command to the peers. */
    if (source == APP_DRIVER_SOURCE_RESTORE) return;
    /* Broadcast the toggle — kit peers listening on this NAME react. */
    zc_espnow_send("{{cfg.power_param}}", 0 /* bool */, val.b ? 1 : 0);
    return;
}
case {{cfg.level_param}}: {
    /* A boot-time restore is local state, not a command to the peers. */
    if (source == APP_DRIVER_SOURCE_RESTORE) return;
    zc_espnow_send("{{cfg.level_param}}", 1 /* u8 */, val.u8);
    return;
}
