{
    camera_config_t {{prefix_lc}}_cam = {};
    {{prefix_lc}}_cam.pin_pwdn = {{prefix}}_PD_PWDN;
    {{prefix_lc}}_cam.pin_reset = {{prefix}}_PD_RESET;
    {{prefix_lc}}_cam.pin_xclk = {{prefix}}_PD_XCLK;
    {{prefix_lc}}_cam.pin_sccb_sda = {{prefix}}_PD_SDA;
    {{prefix_lc}}_cam.pin_sccb_scl = {{prefix}}_PD_SCL;
    {{prefix_lc}}_cam.pin_d7 = {{prefix}}_PD_D7;
    {{prefix_lc}}_cam.pin_d6 = {{prefix}}_PD_D6;
    {{prefix_lc}}_cam.pin_d5 = {{prefix}}_PD_D5;
    {{prefix_lc}}_cam.pin_d4 = {{prefix}}_PD_D4;
    {{prefix_lc}}_cam.pin_d3 = {{prefix}}_PD_D3;
    {{prefix_lc}}_cam.pin_d2 = {{prefix}}_PD_D2;
    {{prefix_lc}}_cam.pin_d1 = {{prefix}}_PD_D1;
    {{prefix_lc}}_cam.pin_d0 = {{prefix}}_PD_D0;
    {{prefix_lc}}_cam.pin_vsync = {{prefix}}_PD_VSYNC;
    {{prefix_lc}}_cam.pin_href = {{prefix}}_PD_HREF;
    {{prefix_lc}}_cam.pin_pclk = {{prefix}}_PD_PCLK;
    {{prefix_lc}}_cam.xclk_freq_hz = {{prefix}}_PD_XCLK_HZ;
    /* LEDC timer/channel 3/7, matching drivers/camera_motion_dvp so the two
     * camera blocks cannot fight over the same PWM resources. */
    {{prefix_lc}}_cam.ledc_timer = LEDC_TIMER_3;
    {{prefix_lc}}_cam.ledc_channel = LEDC_CHANNEL_7;
    {{prefix_lc}}_cam.pixel_format = PIXFORMAT_GRAYSCALE;
    /* The model's input size, straight from the sensor — no resize step. */
    {{prefix_lc}}_cam.frame_size = FRAMESIZE_96X96;
    {{prefix_lc}}_cam.fb_count = 1;
    {{prefix_lc}}_cam.fb_location = CAMERA_FB_IN_DRAM;
    {{prefix_lc}}_cam.grab_mode = CAMERA_GRAB_LATEST;

    esp_err_t {{prefix_lc}}_err = esp_camera_init(&{{prefix_lc}}_cam);
    if ({{prefix_lc}}_err != ESP_OK) {
        /* A missing camera is a degraded state, not a boot blocker — the rest
         * of the product still runs. Same stance as camera_motion_dvp. */
        ESP_LOGE(TAG, "{{prefix_lc}}: camera init failed (%s) — person detection disabled",
                 esp_err_to_name({{prefix_lc}}_err));
    } else {
        zc_tflite_session_cfg_t {{prefix_lc}}_cfg = {
            .model_data     = g_person_detect_model_data,
            .arena_bytes    = {{prefix}}_PD_ARENA_BYTES,
            .arena_in_psram = ({{cfg.arena_in_psram}} != 0),
            .ops            = ZC_TFLITE_OPS_CNN,
            .name           = "person_detect",   /* models/person_detect.yml */
        };
        s_{{prefix_lc}}_session = zc_tflite_session_create(&{{prefix_lc}}_cfg);
        if (s_{{prefix_lc}}_session != NULL) {
            /* 4 KB is not enough for this one: the interpreter recurses through
             * the graph on the calling task's stack. */
            xTaskCreate({{prefix_lc}}_task, "{{prefix_lc}}_pd", 6144, NULL, 5, NULL);
            ESP_LOGI(TAG, "{{prefix_lc}}: person detection every %d ms",
                     {{prefix}}_PD_INTERVAL_MS);
        } else {
            ESP_LOGE(TAG, "{{prefix_lc}}: inference disabled — session create failed");
        }
    }
}
