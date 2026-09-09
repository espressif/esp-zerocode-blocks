if (param_id == {{cfg.motor_param}}) {
    /* Reflect motor activity to ClosureControl MainState (a plain enum8
     * attribute — NOT managed-internally, so attribute::update is correct and
     * takes the CHIP stack lock itself): Moving while driven, Stopped at rest.
     * Fires for controller commands and local changes alike (the delegate and
     * any button write the param with APP_DRIVER_SOURCE_LOCAL). */
    esp_matter_attr_val_t mval = esp_matter_enum8((uint8_t) chip::to_underlying(
        val.i16 != 0 ? chip::app::Clusters::ClosureControl::MainStateEnum::kMoving
                     : chip::app::Clusters::ClosureControl::MainStateEnum::kStopped));
    esp_matter::attribute::update(s_{{prefix_lc}}_endpoint_id,
        chip::app::Clusters::ClosureControl::Id,
        chip::app::Clusters::ClosureControl::Attributes::MainState::Id, &mval);
}
