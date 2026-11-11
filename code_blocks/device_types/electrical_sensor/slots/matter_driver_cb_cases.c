if (param_id == {{cfg.active_power_param}}) {
    /* Code-driven cluster: hand the value to the Delegate (the EPM server
     * reads it back via GetActivePower()) and tell the stack it changed. */
    s_{{prefix_lc}}_epm_delegate.active_power_mw = (int64_t)val.u32;
    {{prefix_lc}}_epm_report(chip::app::Clusters::ElectricalPowerMeasurement::Attributes::ActivePower::Id);
}
{{#if cfg.voltage_param}}
if (param_id == {{cfg.voltage_param}}) {
    s_{{prefix_lc}}_epm_delegate.voltage_mv.SetNonNull((int64_t)val.u32);
    {{prefix_lc}}_epm_report(chip::app::Clusters::ElectricalPowerMeasurement::Attributes::Voltage::Id);
}
{{/if}}
{{#if cfg.current_param}}
if (param_id == {{cfg.current_param}}) {
    s_{{prefix_lc}}_epm_delegate.current_ma.SetNonNull((int64_t)val.u32);
    {{prefix_lc}}_epm_report(chip::app::Clusters::ElectricalPowerMeasurement::Attributes::ActiveCurrent::Id);
}
{{/if}}
