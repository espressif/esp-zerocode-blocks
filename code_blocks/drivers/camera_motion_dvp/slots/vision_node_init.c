{
    camera_config_t {{prefix_lc}}_cam = {};
    {{prefix_lc}}_cam.pin_pwdn = {{prefix}}_CAM_PWDN;
    {{prefix_lc}}_cam.pin_reset = {{prefix}}_CAM_RESET;
    {{prefix_lc}}_cam.pin_xclk = {{prefix}}_CAM_XCLK;
    {{prefix_lc}}_cam.pin_sccb_sda = {{prefix}}_CAM_SDA;
    {{prefix_lc}}_cam.pin_sccb_scl = {{prefix}}_CAM_SCL;
    {{prefix_lc}}_cam.pin_d7 = {{prefix}}_CAM_D7;
    {{prefix_lc}}_cam.pin_d6 = {{prefix}}_CAM_D6;
    {{prefix_lc}}_cam.pin_d5 = {{prefix}}_CAM_D5;
    {{prefix_lc}}_cam.pin_d4 = {{prefix}}_CAM_D4;
    {{prefix_lc}}_cam.pin_d3 = {{prefix}}_CAM_D3;
    {{prefix_lc}}_cam.pin_d2 = {{prefix}}_CAM_D2;
    {{prefix_lc}}_cam.pin_d1 = {{prefix}}_CAM_D1;
    {{prefix_lc}}_cam.pin_d0 = {{prefix}}_CAM_D0;
    {{prefix_lc}}_cam.pin_vsync = {{prefix}}_CAM_VSYNC;
    {{prefix_lc}}_cam.pin_href = {{prefix}}_CAM_HREF;
    {{prefix_lc}}_cam.pin_pclk = {{prefix}}_CAM_PCLK;
    {{prefix_lc}}_cam.xclk_freq_hz = {{prefix}}_CAM_XCLK_HZ;
    {{prefix_lc}}_cam.ledc_timer = LEDC_TIMER_3;
    {{prefix_lc}}_cam.ledc_channel = LEDC_CHANNEL_7;
    {{prefix_lc}}_cam.pixel_format = PIXFORMAT_GRAYSCALE;
    {{prefix_lc}}_cam.frame_size = FRAMESIZE_QQVGA;
    {{prefix_lc}}_cam.fb_count = 1;
    {{prefix_lc}}_cam.fb_location = CAMERA_FB_IN_DRAM;
    {{prefix_lc}}_cam.grab_mode = CAMERA_GRAB_LATEST;

    esp_err_t {{prefix_lc}}_err = esp_camera_init(&{{prefix_lc}}_cam);
    if ({{prefix_lc}}_err != ESP_OK) {
        /* No camera is a degraded state, not a boot blocker. */
        ESP_LOGE(TAG, "{{prefix_lc}}: camera init failed (%s) — motion detection disabled",
                 esp_err_to_name({{prefix_lc}}_err));
    } else {
        xTaskCreate({{prefix_lc}}_motion_task, "{{prefix_lc}}_motion", 4096, NULL, 5, NULL);
        ESP_LOGI(TAG, "Camera {{prefix_lc}}: motion detection at %dx%d, poll %d ms",
                 {{prefix}}_CAM_W, {{prefix}}_CAM_H, {{prefix}}_CAM_POLL_MS);
    }
}
