/* Echo timing by interrupt: the ISR stamps both edges of ECHO and wakes the
 * task on the falling one. Nothing busy-waits, so a missing echo costs the
 * product nothing but this task's 40 ms sleep. */
static volatile int64_t s_{{prefix_lc}}_rise_us = 0;
static volatile int64_t s_{{prefix_lc}}_fall_us = 0;
static TaskHandle_t s_{{prefix_lc}}_hc_task = NULL;
static uint32_t s_{{prefix_lc}}_hc_failures = 0;

static void IRAM_ATTR {{prefix_lc}}_hc_echo_isr(void *arg)
{
    int64_t now = esp_timer_get_time();
    if (gpio_get_level((gpio_num_t){{prefix}}_HC_ECHO_GPIO)) {
        s_{{prefix_lc}}_rise_us = now;
    } else if (s_{{prefix_lc}}_rise_us != 0) {
        s_{{prefix_lc}}_fall_us = now;
        BaseType_t hp = pdFALSE;
        vTaskNotifyGiveFromISR(s_{{prefix_lc}}_hc_task, &hp);
        if (hp == pdTRUE) portYIELD_FROM_ISR();
    }
}

static void {{prefix_lc}}_hc_status(bool ok)
{
{{#if cfg.status_param}}
    app_driver_param_val_t sv = { .b = ok };
    app_driver_set_param({{cfg.status_param}}, sv, APP_DRIVER_SOURCE_LOCAL);
{{/if}}
    (void)ok;
}

static void {{prefix_lc}}_hc_measure(void)
{
    s_{{prefix_lc}}_rise_us = 0;
    s_{{prefix_lc}}_fall_us = 0;
    (void)ulTaskNotifyTake(pdTRUE, 0);   /* drop a stale wake-up */
    gpio_set_level((gpio_num_t){{prefix}}_HC_TRIG_GPIO, 0);
    esp_rom_delay_us(2);
    gpio_set_level((gpio_num_t){{prefix}}_HC_TRIG_GPIO, 1);
    esp_rom_delay_us(10);
    gpio_set_level((gpio_num_t){{prefix}}_HC_TRIG_GPIO, 0);

    /* The module raises ECHO ~0.5 ms after the trigger and holds it for at
       most ~38 ms (out of range); 40 ms covers both plus a tick of slack. */
    TickType_t wait = pdMS_TO_TICKS(40);
    bool got = ulTaskNotifyTake(pdTRUE, wait ? wait + 1 : 2) > 0;
    int64_t rise = s_{{prefix_lc}}_rise_us, fall = s_{{prefix_lc}}_fall_us;
    if (!got || rise == 0 || fall <= rise) {
        s_{{prefix_lc}}_hc_failures++;
        uint32_t per_minute = 60000UL / (uint32_t){{prefix}}_HC_POLL_MS;
        if (per_minute == 0) per_minute = 1;
        if (s_{{prefix_lc}}_hc_failures == 1 || (s_{{prefix_lc}}_hc_failures % per_minute) == 0) {
            ESP_LOGW(TAG, "HC-SR04 {{prefix_lc}}: no echo (%s), %lu consecutive",
                     rise == 0 ? "never rose — sensor absent or unpowered" : "out of range",
                     (unsigned long)s_{{prefix_lc}}_hc_failures);
        }
        {{prefix_lc}}_hc_status(false);
        return;
    }
    if (s_{{prefix_lc}}_hc_failures) {
        ESP_LOGI(TAG, "HC-SR04 {{prefix_lc}}: echo again after %lu miss(es)", (unsigned long)s_{{prefix_lc}}_hc_failures);
        s_{{prefix_lc}}_hc_failures = 0;
    }
    uint32_t dur_us = (uint32_t)(fall - rise);
    /* mm = us * 343 / 2000 (speed of sound 343 m/s, there and back) */
    uint32_t mm = (dur_us * 343U) / 2000U;
    if (mm > 0xFFFF) mm = 0xFFFF;
    app_driver_param_val_t v = { .u16 = (uint16_t)mm };
    app_driver_set_param({{cfg.distance_mm_param}}, v, APP_DRIVER_SOURCE_LOCAL);
    {{prefix_lc}}_hc_status(true);
}

static void {{prefix_lc}}_hc_task_fn(void *arg)
{
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        {{prefix_lc}}_hc_measure();
        TickType_t period = pdMS_TO_TICKS({{prefix}}_HC_POLL_MS);
        vTaskDelayUntil(&last, period ? period : 1);
    }
}
