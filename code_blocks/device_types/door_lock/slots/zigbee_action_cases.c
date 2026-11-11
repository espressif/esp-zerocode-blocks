/* {{prefix_lc}}: a hub's Lock Door / Unlock Door arrive as Door Lock cluster
 * COMMANDS, not attribute writes, so they never reach zigbee_attr_cb_cases.
 * Drive the lock and confirm LockState here. This runs ON the Zigbee task (no
 * lock needed), and the bus never echoes our own source back to
 * zigbee_driver_cb, so LockState is written directly. */
if ((callback_id == EZB_ZCL_CORE_DOOR_LOCK_LOCK_DOOR_CB_ID ||
     callback_id == EZB_ZCL_CORE_DOOR_LOCK_UNLOCK_DOOR_CB_ID) && message) {
    ezb_zcl_door_lock_lock_door_message_t *msg = (ezb_zcl_door_lock_lock_door_message_t *)message;
    if (s_{{prefix_lc}}_zb_endpoint && msg->info.dst_ep == s_{{prefix_lc}}_zb_endpoint) {
        bool lock = callback_id == EZB_ZCL_CORE_DOOR_LOCK_LOCK_DOOR_CB_ID;
        app_driver_param_val_t pv = { .b = lock };
        app_driver_set_param({{cfg.locked_param}}, pv, s_handle);
        /* ZCL LockState: 1 = Locked, 2 = Unlocked */
        uint8_t lock_state = lock ? 1 : 2;
        (void)ezb_zcl_set_attr_value(s_{{prefix_lc}}_zb_endpoint,
            EZB_ZCL_CLUSTER_ID_DOOR_LOCK, EZB_ZCL_CLUSTER_SERVER,
            EZB_ZCL_ATTR_DOOR_LOCK_LOCK_STATE_ID, EZB_ZCL_STD_MANUF_CODE, &lock_state, false);
        msg->out.result = EZB_ZCL_STATUS_SUCCESS;
        return;
    }
}
