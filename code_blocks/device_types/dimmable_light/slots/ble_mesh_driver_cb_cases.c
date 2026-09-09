if (param_id == {{cfg.power_param}}) {
    zc_ble_mesh_set_onoff_state(val.b);
}
if (param_id == {{cfg.brightness_param}}) {
    /* Driver brightness is 0-254; Generic Level is a signed 16-bit range.
     * zc_ble_mesh_level_from_u8 is the one conversion, used in both
     * directions, so a round trip lands back on the value it started from. */
    zc_ble_mesh_set_level_state(zc_ble_mesh_level_from_u8(val.u8));
}
