if (param_id == {{cfg.power_param}}) {
    /* A controller SENDS: the command goes out through the Generic OnOff
     * client, to the group every ZeroCode mesh node's servers subscribe to. */
    zc_ble_mesh_publish_onoff(ZC_MESH_GROUP_ADDR, val.b);
}
if (param_id == {{cfg.level_param}}) {
    zc_ble_mesh_publish_level(ZC_MESH_GROUP_ADDR, zc_ble_mesh_level_from_u8(val.u8));
}
