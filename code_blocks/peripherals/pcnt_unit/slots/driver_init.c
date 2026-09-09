{
    pcnt_unit_config_t {{prefix_lc}}_cfg = {
        .high_limit = {{cfg.high_limit}},
        .low_limit = {{cfg.low_limit}},
        .flags = { .accum_count = 0 },
    };
    ESP_ERROR_CHECK(pcnt_new_unit(&{{prefix_lc}}_cfg, &s_{{prefix_lc}}_pcnt));
    pcnt_unit_enable(s_{{prefix_lc}}_pcnt);
    pcnt_unit_clear_count(s_{{prefix_lc}}_pcnt);
    pcnt_unit_start(s_{{prefix_lc}}_pcnt);
    ESP_LOGI(TAG, "PCNT unit {{prefix_lc}} initialized");
}
