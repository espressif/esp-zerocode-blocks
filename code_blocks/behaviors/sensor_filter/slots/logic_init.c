{
    if (app_driver_poll_every(&{{prefix_lc}}_flt_sample, NULL, (uint32_t){{prefix}}_FLT_PERIOD_MS) != ESP_OK) {
        ESP_LOGE(TAG, "{{prefix_lc}}: could not start sampling {{cfg.source_param}} — {{cfg.output_param}} stays unwritten");
    } else {
        ESP_LOGI(TAG, "{{prefix_lc}}: {{cfg.mode}} of {{cfg.source_param}} over %d samples every %d ms -> {{cfg.output_param}}",
                 {{prefix}}_FLT_SAMPLES, {{prefix}}_FLT_PERIOD_MS);
    }
}
