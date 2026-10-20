{
#if !SOC_PCNT_SUPPORTED
#error "peripherals/pcnt_unit: this chip has no PCNT (pulse counter) peripheral"
#endif
    pcnt_unit_config_t {{prefix_lc}}_cfg = {};
    {{prefix_lc}}_cfg.high_limit = {{cfg.high_limit}};
    {{prefix_lc}}_cfg.low_limit = {{cfg.low_limit}};
    /* Accumulate across the limits — the hardware counter is 16 bits. The
       driver only accumulates at a limit that is a watch point. */
    {{prefix_lc}}_cfg.flags.accum_count = 1;
    ESP_ERROR_CHECK(pcnt_new_unit(&{{prefix_lc}}_cfg, &s_{{prefix_lc}}_pcnt));
    ESP_ERROR_CHECK(pcnt_unit_add_watch_point(s_{{prefix_lc}}_pcnt, {{cfg.high_limit}}));
    ESP_ERROR_CHECK(pcnt_unit_add_watch_point(s_{{prefix_lc}}_pcnt, {{cfg.low_limit}}));
    /* NOT enabled or started: channels can only be added to a unit in the
       init state. The consumer enables + starts it after adding them. */
    ESP_LOGI(TAG, "PCNT unit {{prefix_lc}} created (consumer adds channels, then enables + starts)");
}
