if (param_id == {{cfg.power_param}}) {
    s_from_driver = true;
    esp_matter_attr_val_t mval = esp_matter_bool(val.b);
    esp_matter::attribute::update(
        s_{{prefix_lc}}_endpoint_id,
        chip::app::Clusters::OnOff::Id,
        chip::app::Clusters::OnOff::Attributes::OnOff::Id,
        &mval);
    s_from_driver = false;
}
if (param_id == {{cfg.brightness_param}}) {
    s_from_driver = true;
    /* LevelControl CurrentLevel is 1..254 (0 => CONSTRAINT_ERROR, 0xFF => null sentinel). Clamp the driver value. */
    uint8_t {{prefix_lc}}_lvl = val.u8 < 1 ? 1 : (val.u8 > 254 ? 254 : val.u8);
    esp_matter_attr_val_t mval = esp_matter_nullable_uint8({{prefix_lc}}_lvl);
    esp_matter::attribute::update(
        s_{{prefix_lc}}_endpoint_id,
        chip::app::Clusters::LevelControl::Id,
        chip::app::Clusters::LevelControl::Attributes::CurrentLevel::Id,
        &mval);
    s_from_driver = false;
}
