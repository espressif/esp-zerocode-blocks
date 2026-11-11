/* One PRESS = one Toggle, and a press is an EVENT (block contract:
 * drivers/momentary_button fires it per click). The same param written as
 * state — a restore at boot, an app keeping bookkeeping in it — is not a press
 * and must never switch the room. A position of 0 carries no press. */
if (param_id == {{cfg.position_param}} && val.u8 != 0 && app_driver_notifying_event()) {
    /* Generic OnOff has no toggle value: the framework inverts the state the
     * lights last reported (or this node last sent). */
    zc_ble_mesh_publish_toggle(ZC_MESH_GROUP_ADDR);
}
