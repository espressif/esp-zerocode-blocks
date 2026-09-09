if (param_id == {{cfg.power_param}}) {
    s_from_driver = true;
    esp_matter_attr_val_t mval = esp_matter_bool(val.b);
    esp_matter::attribute::update(s_{{prefix_lc}}_endpoint_id,
        chip::app::Clusters::OnOff::Id,
        chip::app::Clusters::OnOff::Attributes::OnOff::Id, &mval);
    s_from_driver = false;
}
if (param_id == {{cfg.cool_setpoint_param}}) {
    s_from_driver = true;
    esp_matter_attr_val_t mval = esp_matter_int16(val.i16);
    esp_matter::attribute::update(s_{{prefix_lc}}_endpoint_id,
        chip::app::Clusters::Thermostat::Id,
        chip::app::Clusters::Thermostat::Attributes::OccupiedCoolingSetpoint::Id, &mval);
    s_from_driver = false;
}
if (param_id == {{cfg.fan_speed_param}}) {
    s_from_driver = true;
    uint8_t pct = val.u8 > 100 ? 100 : val.u8;
    esp_matter_attr_val_t mval = esp_matter_nullable_uint8(pct);
    esp_matter::attribute::update(s_{{prefix_lc}}_endpoint_id,
        chip::app::Clusters::FanControl::Id,
        chip::app::Clusters::FanControl::Attributes::PercentSetting::Id, &mval);
    s_from_driver = false;
}
