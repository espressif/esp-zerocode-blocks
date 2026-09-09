static esp_timer_handle_t s_{{prefix_lc}}_off_timer = NULL;

/* One-shot: the pulse always ends by itself. Driving the pin inactive here (and
   not tracking any state) means a missed callback can only ever leave the motor
   OFF, never stuck on. */
static void {{prefix_lc}}_off_cb(void *arg)
{
    (void)arg;
    gpio_set_level((gpio_num_t){{prefix}}_HAPTIC_GPIO, !{{prefix}}_HAPTIC_ACTIVE);
}

static void {{prefix_lc}}_buzz(void)
{
    if (!s_{{prefix_lc}}_off_timer) return;
    /* Restart, don't queue: a second trigger during a buzz extends the current
       one rather than scheduling another. esp_timer_stop on an idle timer
       returns ESP_ERR_INVALID_STATE, which is expected and ignored. */
    esp_timer_stop(s_{{prefix_lc}}_off_timer);
    gpio_set_level((gpio_num_t){{prefix}}_HAPTIC_GPIO, {{prefix}}_HAPTIC_ACTIVE);
    esp_timer_start_once(s_{{prefix_lc}}_off_timer, (uint64_t){{prefix}}_HAPTIC_PULSE_MS * 1000ULL);
}
