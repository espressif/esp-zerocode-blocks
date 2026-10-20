if (endpoint_id == s_{{prefix_lc}}_endpoint_id &&
    cluster_id == chip::app::Clusters::Thermostat::Id) {
    if (attribute_id == chip::app::Clusters::Thermostat::Attributes::SystemMode::Id) {
        /* Power is SystemMode: Off (0) = off, anything else (Heat) = on. */
        app_driver_param_val_t pv = { .b = val->val.u8 != 0 };
        app_driver_set_param({{cfg.power_param}}, pv, s_handle);
    } else if (attribute_id == chip::app::Clusters::Thermostat::Attributes::OccupiedHeatingSetpoint::Id) {
        app_driver_param_val_t pv = { .i16 = val->val.i16 };
        app_driver_set_param({{cfg.setpoint_param}}, pv, s_handle);
    }
}
