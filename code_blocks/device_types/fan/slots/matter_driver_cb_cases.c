if (param_id == {{cfg.speed_param}}) {
    s_from_driver = true;
    uint8_t pct = val.u8 > 100 ? 100 : val.u8;
    esp_matter_attr_val_t mval = esp_matter_nullable_uint8(pct);
    esp_matter::attribute::update(
        s_{{prefix_lc}}_endpoint_id,
        chip::app::Clusters::FanControl::Id,
        chip::app::Clusters::FanControl::Attributes::PercentSetting::Id,
        &mval);
    s_from_driver = false;
}
