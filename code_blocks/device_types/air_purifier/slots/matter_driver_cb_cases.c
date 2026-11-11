if (param_id == {{cfg.speed_param}}) {
    /* PercentSetting + PercentCurrent follow the driver, on the Matter thread. */
    s_{{prefix_lc}}_fan.Reflect();
}
