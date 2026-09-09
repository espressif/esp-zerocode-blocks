if (param_id == {{cfg.local_temp_param}}) {
    s_from_driver = true;
    esp_matter_attr_val_t mval = esp_matter_nullable_int16(val.i16);
    esp_matter::attribute::update(s_{{prefix_lc}}_endpoint_id,
        chip::app::Clusters::Thermostat::Id,
        chip::app::Clusters::Thermostat::Attributes::LocalTemperature::Id, &mval);
    s_from_driver = false;
}
if (param_id == {{cfg.heat_setpoint_param}}) {
    s_from_driver = true;
    esp_matter_attr_val_t mval = esp_matter_int16(val.i16);
    esp_matter::attribute::update(s_{{prefix_lc}}_endpoint_id,
        chip::app::Clusters::Thermostat::Id,
        chip::app::Clusters::Thermostat::Attributes::OccupiedHeatingSetpoint::Id, &mval);
    s_from_driver = false;
}
if (param_id == {{cfg.system_mode_param}}) {
    s_from_driver = true;
    esp_matter_attr_val_t mval = esp_matter_enum8(val.u8);
    esp_matter::attribute::update(s_{{prefix_lc}}_endpoint_id,
        chip::app::Clusters::Thermostat::Id,
        chip::app::Clusters::Thermostat::Attributes::SystemMode::Id, &mval);
    s_from_driver = false;
}
