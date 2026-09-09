case {{cfg.position_param}}: {
    /* Any position change fires the configured consumer usage — the
     * classic use is play/pause on a single scene button. */
    zc_ble_hid_send_consumer({{cfg.ble_hid_usage}});
    return;
}
