if (param_id == {{cfg.power_param}}) {
    /* A local change (a button, a schedule, another solution) is this node's
     * OWN state, so it goes into the Generic OnOff server and is published as
     * a status — not sent as a command to somebody else. */
    zc_ble_mesh_set_onoff_state(val.b);
}
