if (param_id == {{cfg.power_param}}) {
    /* No OnOff cluster on a Matter water heater — power is SystemMode Off/Heat. */
    s_from_driver = true;
    esp_matter_attr_val_t mval = esp_matter_enum8(val.b ? 4 : 0);
    esp_matter::attribute::update(s_{{prefix_lc}}_endpoint_id,
        chip::app::Clusters::Thermostat::Id,
        chip::app::Clusters::Thermostat::Attributes::SystemMode::Id, &mval);
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
