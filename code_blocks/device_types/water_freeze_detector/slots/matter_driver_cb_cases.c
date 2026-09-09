/* attribute::update() takes the CHIP stack lock internally — safe to call here even though driver callbacks run off the Matter event-loop thread. If you extend this block, keep using attribute::update; do NOT swap in a raw *Server::Set* cluster setter from a poll timer / console / ISR (no lock → AssertChipStackLockedByCurrentThread abort). Wrap a raw setter in esp_matter::lock::chip_stack_lock() or PlatformMgr().ScheduleWork() only if truly needed. */
if (param_id == {{cfg.freeze_param}}) {
    s_from_driver = true;
    esp_matter_attr_val_t mval = esp_matter_bool(val.b);
    esp_matter::attribute::update(s_{{prefix_lc}}_endpoint_id,
        chip::app::Clusters::BooleanState::Id,
        chip::app::Clusters::BooleanState::Attributes::StateValue::Id, &mval);
    s_from_driver = false;
}
