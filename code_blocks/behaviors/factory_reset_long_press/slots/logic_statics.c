static button_handle_t s_{{prefix_lc}}_fr_btn = NULL;

/* Hold feedback: app_factory_reset_hold_started() once the press has lasted
 * {{prefix}}_FR_CUE_MS (a one-shot esp_timer from press-down — iot_button
 * never fires a per-callback long press shorter than the DEVICE's long-press
 * time, which is whoever created the shared device's), and
 * app_factory_reset_hold_cancelled() on a release before the reset. All of it
 * runs on the esp_timer task, as the button callbacks do, so the flags need
 * no lock. */
static esp_timer_handle_t s_{{prefix_lc}}_fr_cue = NULL;
static bool s_{{prefix_lc}}_fr_held = false;   /* started was called for this press */
static bool s_{{prefix_lc}}_fr_fired = false;  /* the reset fired during this press */

static void {{prefix_lc}}_fr_cue_cb(void *arg)
{
    (void)arg;
    if (s_{{prefix_lc}}_fr_held || s_{{prefix_lc}}_fr_fired) return;
    s_{{prefix_lc}}_fr_held = true;
    ESP_LOGI(TAG, "{{prefix_lc}}: held %u ms — factory reset at %u ms unless released", (unsigned){{prefix}}_FR_CUE_MS, (unsigned){{prefix}}_FR_HOLD_MS);
    app_factory_reset_hold_started();
}

static void {{prefix_lc}}_fr_down_cb(void *arg, void *usr_data)
{
    (void)arg; (void)usr_data;
    s_{{prefix_lc}}_fr_held = false;
    s_{{prefix_lc}}_fr_fired = false;
    if (s_{{prefix_lc}}_fr_cue) {
        esp_timer_stop(s_{{prefix_lc}}_fr_cue);
        esp_timer_start_once(s_{{prefix_lc}}_fr_cue, (uint64_t){{prefix}}_FR_CUE_MS * 1000ULL);
    }
}

static void {{prefix_lc}}_fr_up_cb(void *arg, void *usr_data)
{
    (void)arg; (void)usr_data;
    if (s_{{prefix_lc}}_fr_cue) esp_timer_stop(s_{{prefix_lc}}_fr_cue);
    if (s_{{prefix_lc}}_fr_held && !s_{{prefix_lc}}_fr_fired) {
        ESP_LOGI(TAG, "{{prefix_lc}}: released before the factory reset — cancelled");
        app_factory_reset_hold_cancelled();
    }
    s_{{prefix_lc}}_fr_held = false;
    s_{{prefix_lc}}_fr_fired = false;
}

static void {{prefix_lc}}_factory_reset_cb(void *arg, void *usr_data)
{
    (void)arg; (void)usr_data;
    if (s_{{prefix_lc}}_fr_cue) esp_timer_stop(s_{{prefix_lc}}_fr_cue);
    if (!s_{{prefix_lc}}_fr_held) {
        s_{{prefix_lc}}_fr_held = true;
        app_factory_reset_hold_started();
    }
    s_{{prefix_lc}}_fr_fired = true;
    ESP_LOGW(TAG, "{{prefix_lc}}: long-press detected — factory reset");
    if (app_utils_factory_reset() != ESP_OK) {
        /* The erase failed and the device carries on: end the feedback. */
        app_factory_reset_hold_cancelled();
    }
}
