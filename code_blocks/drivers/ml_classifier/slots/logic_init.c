{
    /* PREPARE once. A failure is logged by the framework and leaves the
     * session NULL, so the product keeps booting rather than dying on a
     * model problem — and the task is never started. */
    zc_tflite_session_cfg_t {{prefix_lc}}_cfg = {
        .model_data     = {{cfg.model_symbol}},
        .arena_bytes    = {{prefix}}_TFL_ARENA_BYTES,
        .arena_in_psram = ({{cfg.arena_in_psram}} != 0),
        .ops            = ZC_TFLITE_OPS_ALIAS_{{cfg.ops_profile}},
        .name           = "{{cfg.model_symbol}}",
    };
    s_{{prefix_lc}}_session = zc_tflite_session_create(&{{prefix_lc}}_cfg);
    if (s_{{prefix_lc}}_session != NULL) {
        /* 4096 suits the default reference model (one FullyConnected). The
         * interpreter recurses through the graph on THIS task's stack, so a
         * deeper model needs a deeper task — drivers/ml_person_detect uses
         * 6144 for its CNN. Raise this alongside model_symbol. */
        xTaskCreate({{prefix_lc}}_task, "{{prefix_lc}}_tfl", 4096, NULL, 5, NULL);
        ESP_LOGI(TAG, "{{prefix_lc}}: classifier every %d ms", {{prefix}}_TFL_INTERVAL_MS);
    } else {
        ESP_LOGE(TAG, "{{prefix_lc}}: inference disabled — session create failed");
    }
}
