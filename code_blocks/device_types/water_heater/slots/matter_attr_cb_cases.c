if (endpoint_id == s_{{prefix_lc}}_endpoint_id) {
    if (cluster_id == chip::app::Clusters::OnOff::Id &&
        attribute_id == chip::app::Clusters::OnOff::Attributes::OnOff::Id) {
        app_driver_param_val_t pv = { .b = val->val.b };
        app_driver_set_param({{cfg.power_param}}, pv, s_handle);
    } else if (cluster_id == chip::app::Clusters::Thermostat::Id &&
               attribute_id == chip::app::Clusters::Thermostat::Attributes::OccupiedHeatingSetpoint::Id) {
        app_driver_param_val_t pv = { .i16 = val->val.i16 };
        app_driver_set_param({{cfg.setpoint_param}}, pv, s_handle);
    }
}
