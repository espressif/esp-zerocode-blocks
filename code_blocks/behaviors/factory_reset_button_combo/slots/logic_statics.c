static int64_t s_{{prefix_lc}}_combo_start_us = 0;
static bool s_{{prefix_lc}}_combo_held = false;   /* hold_started was called for this hold */
static bool s_{{prefix_lc}}_combo_fired = false;  /* the reset fired during this hold */
static int64_t s_{{prefix_lc}}_down_a_us = 0;      /* when each press began (0 = up) */
static int64_t s_{{prefix_lc}}_down_b_us = 0;

static void {{prefix_lc}}_combo_poll(void *arg)
{
    int a = gpio_get_level((gpio_num_t){{prefix}}_FR_GPIO_A);
    int b = gpio_get_level((gpio_num_t){{prefix}}_FR_GPIO_B);
    int64_t now = esp_timer_get_time();
    bool down_a = (a == {{prefix}}_FR_LEVEL), down_b = (b == {{prefix}}_FR_LEVEL);
    if (!down_a) s_{{prefix_lc}}_down_a_us = 0; else if (s_{{prefix_lc}}_down_a_us == 0) s_{{prefix_lc}}_down_a_us = now;
    if (!down_b) s_{{prefix_lc}}_down_b_us = 0; else if (s_{{prefix_lc}}_down_b_us == 0) s_{{prefix_lc}}_down_b_us = now;
    /* Together = both down, and the presses began within together_ms of each
       other; once a hold is under way it stays the combo until a release. */
    int64_t apart = s_{{prefix_lc}}_down_a_us - s_{{prefix_lc}}_down_b_us;
    if (apart < 0) apart = -apart;
    bool both = down_a && down_b &&
                (s_{{prefix_lc}}_combo_start_us != 0 || apart <= (int64_t){{prefix}}_FR_TOGETHER_MS * 1000);
    if (!both) {
        if (s_{{prefix_lc}}_combo_held && !s_{{prefix_lc}}_combo_fired) {
            ESP_LOGI(TAG, "{{prefix_lc}}: button combo released before the factory reset — cancelled");
            app_factory_reset_hold_cancelled();
        }
        s_{{prefix_lc}}_combo_start_us = 0;
        s_{{prefix_lc}}_combo_held = false;
        s_{{prefix_lc}}_combo_fired = false;
        return;
    }
    if (s_{{prefix_lc}}_combo_fired) return;   /* a failed erase: wait for a release */
    if (s_{{prefix_lc}}_combo_start_us == 0) s_{{prefix_lc}}_combo_start_us = now;
    int64_t held_us = now - s_{{prefix_lc}}_combo_start_us;
    if (!s_{{prefix_lc}}_combo_held && (held_us >= ((int64_t){{prefix}}_FR_CUE_MS * 1000) || held_us >= ((int64_t){{prefix}}_FR_HOLD_MS * 1000))) {
        s_{{prefix_lc}}_combo_held = true;
        ESP_LOGI(TAG, "{{prefix_lc}}: button combo held — factory reset at %u ms unless released", (unsigned){{prefix}}_FR_HOLD_MS);
        app_factory_reset_hold_started();
    }
    if (held_us >= ((int64_t){{prefix}}_FR_HOLD_MS * 1000)) {
        s_{{prefix_lc}}_combo_fired = true;
        ESP_LOGW(TAG, "{{prefix_lc}}: button combo held — factory reset");
        if (app_utils_factory_reset() != ESP_OK) {
            /* The erase failed and the device carries on: end the feedback. */
            app_factory_reset_hold_cancelled();
        }
    }
}
