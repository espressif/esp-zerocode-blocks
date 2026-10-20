static esp_timer_handle_t s_{{prefix_lc}}_pulse_timer = NULL;
static volatile bool s_{{prefix_lc}}_driving = false;
static volatile bool s_{{prefix_lc}}_cmd_locked = false;
/* Set only by the esp_timer task while it writes the SENSED state back, so the
   apply case can tell that write (bolt already there — do not drive) from a
   command arriving on another task at the same moment (drive). */
static volatile bool s_{{prefix_lc}}_reporting = false;
static TaskHandle_t s_{{prefix_lc}}_report_task = NULL;
static int s_{{prefix_lc}}_last_sensed = -1;

static bool {{prefix_lc}}_sensed_locked(void)
{
    return gpio_get_level((gpio_num_t){{prefix}}_POSITION_GPIO) == {{prefix}}_POSITION_ACTIVE;
}

static void {{prefix_lc}}_report(bool locked)
{
    s_{{prefix_lc}}_report_task = xTaskGetCurrentTaskHandle();
    s_{{prefix_lc}}_reporting = true;
    app_driver_param_val_t v = { .b = locked };
    app_driver_set_param({{cfg.locked_param}}, v, APP_DRIVER_SOURCE_LOCAL);
    s_{{prefix_lc}}_reporting = false;
}

/* End of a motor pulse: release the H-bridge, then believe the switch, not the
   command. */
static void {{prefix_lc}}_pulse_end_cb(void *arg)
{
    (void)arg;
    gpio_set_level((gpio_num_t){{prefix}}_LOCK_GPIO, 0);
    gpio_set_level((gpio_num_t){{prefix}}_UNLOCK_GPIO, 0);
    bool sensed = {{prefix_lc}}_sensed_locked();
    bool cmd = s_{{prefix_lc}}_cmd_locked;
    s_{{prefix_lc}}_last_sensed = sensed;
    s_{{prefix_lc}}_driving = false;
    if (sensed != cmd) {
        ESP_LOGW(TAG, "Lock {{prefix_lc}}: JAM — commanded %s, bolt reads %s",
                 cmd ? "LOCKED" : "UNLOCKED", sensed ? "LOCKED" : "UNLOCKED");
        {{prefix_lc}}_report(sensed);
    } else {
        ESP_LOGI(TAG, "Lock {{prefix_lc}}: %s", sensed ? "LOCKED" : "UNLOCKED");
    }
}

/* Manual turns while the motor is idle. A reading must repeat on the next poll
   before it counts, so a bouncing switch is not a turn. Same task as
   pulse_end_cb (esp_timer), so the two never interleave. */
static void {{prefix_lc}}_position_poll_cb(void *arg)
{
    (void)arg;
    if (s_{{prefix_lc}}_driving) {
        s_{{prefix_lc}}_last_sensed = -1;
        return;
    }
    int sensed = {{prefix_lc}}_sensed_locked() ? 1 : 0;
    if (sensed != s_{{prefix_lc}}_last_sensed) {
        s_{{prefix_lc}}_last_sensed = sensed;
        return;
    }
    app_driver_param_val_t cur;
    if (app_driver_param_seen({{cfg.locked_param}}) &&
        app_driver_get_param({{cfg.locked_param}}, &cur) == ESP_OK &&
        cur.b == (bool)sensed) {
        return;
    }
    ESP_LOGI(TAG, "Lock {{prefix_lc}}: bolt reads %s (turned by hand)", sensed ? "LOCKED" : "UNLOCKED");
    {{prefix_lc}}_report(sensed != 0);
}
