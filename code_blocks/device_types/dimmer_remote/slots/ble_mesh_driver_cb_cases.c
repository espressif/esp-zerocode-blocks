/* A controller SENDS: the command goes out through the Generic OnOff / Level
 * clients, to the group every ZeroCode mesh node's servers subscribe to. A
 * boot-time restore of the remembered value is local state, not a command —
 * replaying it would switch the room on every reboot. */
if (param_id == {{cfg.power_param}} && source != APP_DRIVER_SOURCE_RESTORE) {
    zc_ble_mesh_publish_onoff(ZC_MESH_GROUP_ADDR, val.b);
}
if (param_id == {{cfg.level_param}} && source != APP_DRIVER_SOURCE_RESTORE) {
    zc_ble_mesh_publish_level(ZC_MESH_GROUP_ADDR, zc_ble_mesh_level_from_u8(val.u8));
}
