if (endpoint_id == s_{{prefix_lc}}_endpoint_id &&
    cluster_id == chip::app::Clusters::DoorLock::Id &&
    attribute_id == chip::app::Clusters::DoorLock::Attributes::LockState::Id) {
    bool locked = (val->val.u8 == 1);
    app_driver_param_val_t pv = { .b = locked };
    app_driver_set_param({{cfg.locked_param}}, pv, s_handle);
}
