case {{cfg.position_param}}: {
    /* Each nonzero notification is one press and fires the configured
     * consumer usage — the classic use is play/pause on a single scene
     * button. A position of 0 carries no press (the position_param contract). */
    if (val.u8 != 0) {
        zc_ble_hid_send_consumer({{cfg.ble_hid_usage}});
    }
    return;
}
