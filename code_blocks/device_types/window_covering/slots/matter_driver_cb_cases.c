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
