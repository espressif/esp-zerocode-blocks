/* {{prefix_lc}}: frame-difference motion detector. Previous grayscale frame
 * lives in a static buffer (19 KB) — no PSRAM requirement. */
static uint8_t s_{{prefix_lc}}_prev[{{prefix}}_CAM_W * {{prefix}}_CAM_H];
static bool s_{{prefix_lc}}_have_prev = false;

/* Kept for the retries: filled once by vision_node_init. */
static camera_config_t s_{{prefix_lc}}_cam_cfg;

/* A camera not seated or powered at boot is a degraded state, not a dead
 * feature: retry the init with a growing pause (5 s .. 60 s) until it answers. */
static void {{prefix_lc}}_camera_until_up(void)
{
    uint32_t pause_ms = 5000;
    for (;;) {
        esp_err_t err = esp_camera_init(&s_{{prefix_lc}}_cam_cfg);
        if (err == ESP_OK) return;   /* a failed init released what it took */
        ESP_LOGW(TAG, "{{prefix_lc}}: camera init failed (%s) — retrying in %lu s",
                 esp_err_to_name(err), (unsigned long)(pause_ms / 1000));
        vTaskDelay(pdMS_TO_TICKS(pause_ms));
        if (pause_ms < 60000) pause_ms *= 2;
    }
}

static void {{prefix_lc}}_motion_task(void *arg)
{
    (void)arg;
    {{prefix_lc}}_camera_until_up();
    ESP_LOGI(TAG, "Camera {{prefix_lc}}: motion detection at %dx%d, poll %d ms",
             {{prefix}}_CAM_W, {{prefix}}_CAM_H, {{prefix}}_CAM_POLL_MS);
    bool motion = false;
    int64_t last_motion_ms = 0;
    const int total = {{prefix}}_CAM_W * {{prefix}}_CAM_H;
    const int area_px = (total * {{prefix}}_CAM_AREA_PCT) / 100;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS({{prefix}}_CAM_POLL_MS));
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb == NULL) {
            ESP_LOGW(TAG, "{{prefix_lc}}: frame capture failed");
            continue;
        }
        if (fb->len >= (size_t)total) {
            if (s_{{prefix_lc}}_have_prev) {
                int changed = 0;
                for (int i = 0; i < total; i++) {
                    int d = (int)fb->buf[i] - (int)s_{{prefix_lc}}_prev[i];
                    if (d < 0) d = -d;
                    if (d > {{prefix}}_CAM_PX_THRESH) changed++;
                }
                int64_t now_ms = (int64_t)(esp_timer_get_time() / 1000);
                if (changed > area_px) last_motion_ms = now_ms;
                bool now_motion = (now_ms - last_motion_ms) < {{prefix}}_CAM_HOLD_MS && last_motion_ms != 0;
                if (now_motion != motion) {
                    motion = now_motion;
                    ESP_LOGI(TAG, "{{prefix_lc}}: motion %s (%d px changed)",
                             motion ? "detected" : "cleared", changed);
                    zc_vision_emit(0, motion ? 1 : 0);
                }
            }
            memcpy(s_{{prefix_lc}}_prev, fb->buf, total);
            s_{{prefix_lc}}_have_prev = true;
        }
        esp_camera_fb_return(fb);
    }
}
