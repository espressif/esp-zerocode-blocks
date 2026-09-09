if (param_id == {{cfg.power_param}}) {
    s_from_driver = true;
    esp_matter_attr_val_t mval = esp_matter_bool(val.b);
    esp_matter::attribute::update(s_{{prefix_lc}}_endpoint_id,
        chip::app::Clusters::OnOff::Id,
        chip::app::Clusters::OnOff::Attributes::OnOff::Id, &mval);
    s_from_driver = false;
}
if (param_id == {{cfg.setpoint_param}}) {
    s_from_driver = true;
    esp_matter_attr_val_t mval = esp_matter_int16(val.i16);
    esp_matter::attribute::update(s_{{prefix_lc}}_endpoint_id,
        chip::app::Clusters::Thermostat::Id,
        chip::app::Clusters::Thermostat::Attributes::OccupiedHeatingSetpoint::Id, &mval);
    s_from_driver = false;
}
