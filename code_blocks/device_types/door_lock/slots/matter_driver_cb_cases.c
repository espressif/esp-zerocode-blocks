if (param_id == {{cfg.locked_param}}) {
    s_from_driver = true;
    /* DoorLock::LockState: 1 = Locked, 2 = Unlocked */
    esp_matter_attr_val_t mval = esp_matter_nullable_enum8(val.b ? 1 : 2);
    esp_matter::attribute::update(s_{{prefix_lc}}_endpoint_id,
        chip::app::Clusters::DoorLock::Id,
        chip::app::Clusters::DoorLock::Attributes::LockState::Id, &mval);
    s_from_driver = false;
}
