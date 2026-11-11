/* Target: 5-35 °C in 0.5 °C steps (raw hundredths), the range the mqtt and
 * display bindings offer. The mode toggle is off = 0 Off, on = 4 Heat — the
 * Matter endpoint is heat-only, so a toggle never sends 1 (Auto). */
{ "{{cfg.label}} temperature (°C)", "value", {{cfg.local_temp_param}}, "{{cfg.local_temp_param}}", 2, -4000, 12500, 100 },
{ "{{cfg.label}} target (°C)", "number", {{cfg.heat_setpoint_param}}, "{{cfg.heat_setpoint_param}}", 2, 500, 3500, 100, 50 },
{ "{{cfg.label}} heating mode", "toggle", {{cfg.system_mode_param}}, "{{cfg.system_mode_param}}", 1, 0, 4, 1 },
{{#if cfg.heating_param}}
{ "{{cfg.label}} heating now", "binary", {{cfg.heating_param}}, "{{cfg.heating_param}}", 0, 0, 1, 1 },
{{/if}}
