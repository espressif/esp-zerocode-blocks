if (param_id == {{cfg.position_param}}) {
    /* Generic OnOff has no toggle value: the framework inverts the state the
     * lights last reported (or this node last sent). */
    zc_ble_mesh_publish_toggle(ZC_MESH_GROUP_ADDR);
}
