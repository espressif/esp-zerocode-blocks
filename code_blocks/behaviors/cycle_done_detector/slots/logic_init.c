{
    bool {{prefix_lc}}_resume = false;
    nvs_handle_t {{prefix_lc}}_h;
    if (nvs_open({{prefix}}_CYC_NVS_NS, NVS_READONLY, &{{prefix_lc}}_h) == ESP_OK) {
        uint8_t {{prefix_lc}}_saved = 0;
        if (nvs_get_u8({{prefix_lc}}_h, {{prefix}}_CYC_NVS_KEY, &{{prefix_lc}}_saved) == ESP_OK) {
            {{prefix_lc}}_resume = {{prefix_lc}}_saved != 0;
        }
        nvs_close({{prefix_lc}}_h);
    }
    if ({{prefix_lc}}_resume) {
        /* Restarted mid-cycle: running, minimum run treated as served. */
        s_{{prefix_lc}}_cyc_running = true;
        s_{{prefix_lc}}_cyc_start_s = -(int64_t){{prefix}}_CYC_MIN_RUN_S;
        s_{{prefix_lc}}_cyc_busy_s = 0;
        ESP_LOGI(TAG, "{{prefix_lc}}: restarted during a cycle — still running");
    }
    {{prefix_lc}}_cyc_publish({{prefix_lc}}_resume, false);

    const esp_timer_create_args_t {{prefix_lc}}_cyc_args = {
        .callback = &{{prefix_lc}}_cyc_tick_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "{{prefix_lc}}_cyc",
        .skip_unhandled_events = true,
    };
    esp_timer_handle_t {{prefix_lc}}_cyc_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&{{prefix_lc}}_cyc_args, &{{prefix_lc}}_cyc_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic({{prefix_lc}}_cyc_timer, 1000000ULL));
    ESP_LOGI(TAG, "{{prefix_lc}}: cycle monitor on {{cfg.activity_param}} (min level %d, quiet %d min, min run %d min)",
             {{prefix}}_CYC_MIN_LEVEL, {{prefix}}_CYC_QUIET_S / 60, {{prefix}}_CYC_MIN_RUN_S / 60);
}
