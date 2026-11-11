#if {{prefix}}_WC_SPLIT
{{#if cfg.current_param}}
if (param_id == {{cfg.current_param}}) {
    /* Measured position, percent OPEN; 0xFFFF (or anything past 10000) is
     * unknown and reported as a null CurrentPosition. Matter counts CLOSED. */
    s_from_driver = true;
    esp_matter_attr_val_t mval = val.u16 > 10000
        ? esp_matter_nullable_uint16(nullable<uint16_t>())
        : esp_matter_nullable_uint16((uint16_t)(10000 - val.u16));
    esp_matter::attribute::update(s_{{prefix_lc}}_endpoint_id,
        chip::app::Clusters::WindowCovering::Id,
        chip::app::Clusters::WindowCovering::Attributes::CurrentPositionLiftPercent100ths::Id, &mval);
    s_from_driver = false;
}
{{/if}}
if (param_id == {{cfg.position_param}} && val.u16 <= 10000 && source != s_handle) {
    /* A target set from elsewhere (voice, display, MQTT, RainMaker) shows as
     * Matter's Target; one Matter set itself is already there. */
    s_from_driver = true;
    esp_matter_attr_val_t mval = esp_matter_nullable_uint16((uint16_t)(10000 - val.u16));
    esp_matter::attribute::update(s_{{prefix_lc}}_endpoint_id,
        chip::app::Clusters::WindowCovering::Id,
        chip::app::Clusters::WindowCovering::Attributes::TargetPositionLiftPercent100ths::Id, &mval);
    s_from_driver = false;
}
#else
if (param_id == {{cfg.position_param}}) {
    /* Driver position is percent OPEN; Matter CurrentPosition is percent CLOSED. */
    uint16_t closed = (uint16_t)(10000 - (val.u16 > 10000 ? 10000 : val.u16));
    s_from_driver = true;
    esp_matter_attr_val_t mval = esp_matter_nullable_uint16(closed);
    esp_matter::attribute::update(s_{{prefix_lc}}_endpoint_id,
        chip::app::Clusters::WindowCovering::Id,
        chip::app::Clusters::WindowCovering::Attributes::CurrentPositionLiftPercent100ths::Id, &mval);
    s_from_driver = false;
}
#endif
