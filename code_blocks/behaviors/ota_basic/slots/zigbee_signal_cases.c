if ((sig_type == EZB_BDB_SIGNAL_STEERING &&
     err_status == EZB_BDB_STATUS_SUCCESS) ||
    ((sig_type == EZB_BDB_SIGNAL_DEVICE_FIRST_START ||
      sig_type == EZB_BDB_SIGNAL_DEVICE_REBOOT) &&
     err_status == EZB_BDB_STATUS_SUCCESS && !ezb_bdb_is_factory_new())) {
    /* {{prefix_lc}}: on-network — (re)arm the periodic OTA image query.
     * The worker takes the stack lock and sends one query immediately, then
     * repeats at the same 24-hour cadence used by the previous client. */
    {{prefix_lc}}_zb_ota_arm_query();
}
if (sig_type == EZB_ZDO_SIGNAL_LEAVE) {
    s_{{prefix_lc}}_on_network = false;
}
