if (param_id == {{cfg.position_param}}) {
    s_from_driver = true;
    esp_matter_attr_val_t mval = esp_matter_nullable_uint16(val.u16);
    esp_matter::attribute::update(s_{{prefix_lc}}_endpoint_id,
        chip::app::Clusters::WindowCovering::Id,
        chip::app::Clusters::WindowCovering::Attributes::CurrentPositionLiftPercent100ths::Id, &mval);
    s_from_driver = false;
}
