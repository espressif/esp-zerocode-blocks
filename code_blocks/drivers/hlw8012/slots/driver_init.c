{
    pcnt_unit_config_t {{prefix_lc}}_cfg = { .high_limit = 32767, .low_limit = -1, .flags = { .accum_count = 0 } };
    ESP_ERROR_CHECK(pcnt_new_unit(&{{prefix_lc}}_cfg, &s_{{prefix_lc}}_hlw_pcnt));
    pcnt_chan_config_t {{prefix_lc}}_chan_cfg = {
        .edge_gpio_num = {{prefix}}_HLW_CF_GPIO,
        .level_gpio_num = -1,
        /* ESP-IDF 6 removed `io_loop_back` from EVERY driver's config flags: binding
           two drivers to the same GPIO now just works, so the flag had nothing left
           to do. Setting it is a hard compile error, not a deprecation. */
        .flags = { .invert_edge_input = 0, .invert_level_input = 0, .virt_edge_io_level = 0, .virt_level_io_level = 0 },
    };
    pcnt_channel_handle_t {{prefix_lc}}_chan = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(s_{{prefix_lc}}_hlw_pcnt, &{{prefix_lc}}_chan_cfg, &{{prefix_lc}}_chan));
    pcnt_channel_set_edge_action({{prefix_lc}}_chan, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_HOLD);
    pcnt_channel_set_level_action({{prefix_lc}}_chan, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_KEEP);
    pcnt_unit_enable(s_{{prefix_lc}}_hlw_pcnt);
    pcnt_unit_clear_count(s_{{prefix_lc}}_hlw_pcnt);
    pcnt_unit_start(s_{{prefix_lc}}_hlw_pcnt);
    ESP_LOGI(TAG, "HLW8012 {{prefix_lc}}: CF GPIO %d", {{prefix}}_HLW_CF_GPIO);
}
