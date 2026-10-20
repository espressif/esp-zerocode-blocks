/* +1 opening, -1 closing, 0 idle. Written by apply (caller's task) and by the
   limit poll (esp_timer task), always under s_{{prefix_lc}}_mux together with
   the pins, so a stop for the OLD direction can never switch off a run the
   other task has just started in the new one. */
static int s_{{prefix_lc}}_dir = 0;
static int64_t s_{{prefix_lc}}_started_us = 0;
static portMUX_TYPE s_{{prefix_lc}}_mux = portMUX_INITIALIZER_UNLOCKED;

static void {{prefix_lc}}_motor_off(void)
{
    gpio_set_level((gpio_num_t){{prefix}}_UP_GPIO, 0);
    gpio_set_level((gpio_num_t){{prefix}}_DOWN_GPIO, 0);
}

static bool {{prefix_lc}}_at_limit(int dir)
{
    gpio_num_t pin = (gpio_num_t)(dir > 0 ? {{prefix}}_UPPER_LIMIT_GPIO : {{prefix}}_LOWER_LIMIT_GPIO);
    return gpio_get_level(pin) == {{prefix}}_LIMIT_ACTIVE;
}

/* Runs every 20 ms for the life of the device. A periodic timer that is never
   stopped is simpler to get right than start/stop around each run: there is no
   window where a new run starts and a late stop leaves it unwatched. */
static void {{prefix_lc}}_limit_poll_cb(void *arg)
{
    (void)arg;
    int dir;
    bool reached = false, timed_out = false;
    portENTER_CRITICAL(&s_{{prefix_lc}}_mux);
    dir = s_{{prefix_lc}}_dir;
    if (dir != 0) {
        if ({{prefix_lc}}_at_limit(dir)) {
            reached = true;
        } else if (esp_timer_get_time() - s_{{prefix_lc}}_started_us >
                   (int64_t){{prefix}}_MAX_RUN_MS * 1000LL) {
            timed_out = true;
        }
        if (reached || timed_out) {
            {{prefix_lc}}_motor_off();
            s_{{prefix_lc}}_dir = 0;
        }
    }
    portEXIT_CRITICAL(&s_{{prefix_lc}}_mux);

    if (reached) {
        ESP_LOGI(TAG, "Curtain {{prefix_lc}}: %s limit reached, motor off", dir > 0 ? "upper" : "lower");
        /* Report where the curtain IS. The re-apply this causes finds the
           limit already asserted and does not start the motor. */
        app_driver_param_val_t end = { .u16 = (uint16_t)(dir > 0 ? 10000 : 0) };
        app_driver_set_param({{cfg.position_param}}, end, APP_DRIVER_SOURCE_LOCAL);
    } else if (timed_out) {
        /* Position is unknown, so nothing is published. */
        ESP_LOGE(TAG, "Curtain {{prefix_lc}}: no %s limit after %d ms — motor stopped; check the limit switch",
                 dir > 0 ? "upper" : "lower", {{prefix}}_MAX_RUN_MS);
    }
}
