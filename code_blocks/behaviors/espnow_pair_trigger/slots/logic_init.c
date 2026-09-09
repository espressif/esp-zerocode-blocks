{
    s_{{prefix_lc}}_pair_handle = app_driver_register_solution("{{prefix_lc}}_pair", {{prefix_lc}}_pair_cb, NULL);
    ESP_LOGI(TAG, "{{prefix_lc}}: pairing opens for %d s when {{cfg.trigger_param}} fires", {{prefix}}_PAIR_WINDOW_S);
}
