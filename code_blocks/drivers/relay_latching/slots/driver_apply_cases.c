case {{cfg.param_id}}: {
    {{prefix_lc}}_latch_pulse(val.b);
    ESP_LOGI(TAG, "Latching relay {{prefix_lc}}: %s", val.b ? "ON" : "OFF");
    return ESP_OK;
}
