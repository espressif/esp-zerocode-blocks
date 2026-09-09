case {{cfg.power_param}}: {
    /* Toggle = mute — the remote's "power" button mutes the host. */
    zc_ble_hid_send_consumer(0xE2);
    return;
}
case {{cfg.level_param}}: {
    if (s_{{prefix_lc}}_level_seen) {
        int8_t delta = (int8_t)(val.u8 - s_{{prefix_lc}}_prev_level);
        if (delta > 0) zc_ble_hid_send_consumer(0xE9);      /* volume up */
        else if (delta < 0) zc_ble_hid_send_consumer(0xEA); /* volume down */
    }
    s_{{prefix_lc}}_prev_level = val.u8;
    s_{{prefix_lc}}_level_seen = true;
    return;
}
