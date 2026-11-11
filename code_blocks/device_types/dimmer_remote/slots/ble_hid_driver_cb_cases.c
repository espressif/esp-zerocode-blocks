case {{cfg.power_param}}: {
    /* Toggle = mute — the remote's "power" button mutes the host. */
    zc_ble_hid_send_consumer(0xE2);
    return;
}
case {{cfg.level_param}}: {
    if (s_{{prefix_lc}}_level_seen) {
        /* One key per step of the change: an encoder reports every detent
         * of a fast spin in one write, and one key for all of them made the
         * host move a single step. Capped so a jump in an absolute level
         * cannot hold the callback for long. */
        int8_t delta = (int8_t)(val.u8 - s_{{prefix_lc}}_prev_level);
        int n = delta < 0 ? -delta : delta;
        if (n > 16) n = 16;
        for (int i = 0; i < n; i++) {
            zc_ble_hid_send_consumer(delta > 0 ? 0xE9 /* volume up */ : 0xEA /* volume down */);
        }
    }
    s_{{prefix_lc}}_prev_level = val.u8;
    s_{{prefix_lc}}_level_seen = true;
    return;
}
