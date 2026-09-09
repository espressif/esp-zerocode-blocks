if (param_id == {{cfg.locked_param}} && s_{{prefix_lc}}_zb_endpoint) {
    /* ZCL LockState: 1 = Locked, 2 = Unlocked */
    uint8_t lock_state = val.b ? 1 : 2;
    esp_zigbee_lock_acquire(portMAX_DELAY);
    (void)ezb_zcl_set_attr_value(s_{{prefix_lc}}_zb_endpoint,
        EZB_ZCL_CLUSTER_ID_DOOR_LOCK, EZB_ZCL_CLUSTER_SERVER,
        EZB_ZCL_ATTR_DOOR_LOCK_LOCK_STATE_ID, EZB_ZCL_STD_MANUF_CODE, &lock_state, false);
    esp_zigbee_lock_release();
}
