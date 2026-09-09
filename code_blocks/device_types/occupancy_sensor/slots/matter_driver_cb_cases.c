/* attribute::update() takes the CHIP stack lock internally — safe to call here even though driver callbacks run off the Matter event-loop thread. If you extend this block, keep using attribute::update; do NOT swap in a raw *Server::Set* cluster setter from a poll timer / console / ISR (no lock → AssertChipStackLockedByCurrentThread abort). Wrap a raw setter in esp_matter::lock::chip_stack_lock() or PlatformMgr().ScheduleWork() only if truly needed. */
if (param_id == {{cfg.occupied_param}}) {
    s_from_driver = true;
    /* Occupancy attribute is a bitmap8: bit 0 = occupied */
    esp_matter_attr_val_t mval = esp_matter_bitmap8(val.b ? 1 : 0);
    esp_matter::attribute::update(s_{{prefix_lc}}_endpoint_id,
        chip::app::Clusters::OccupancySensing::Id,
        chip::app::Clusters::OccupancySensing::Attributes::Occupancy::Id, &mval);
    s_from_driver = false;
}
