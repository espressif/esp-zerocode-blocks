static uint8_t s_{{prefix_lc}}_pos = 0;

/* Override in app code to react to encoder steps; default updates position param. */
__attribute__((weak)) void {{prefix_lc}}_on_step(int8_t delta)
{
    s_{{prefix_lc}}_pos = (uint8_t)((int)s_{{prefix_lc}}_pos + delta);
    app_driver_param_val_t v = { .u8 = s_{{prefix_lc}}_pos };
    app_driver_set_param({{cfg.position_param}}, v, APP_DRIVER_SOURCE_LOCAL);
}

#if SOC_PCNT_SUPPORTED
static pcnt_unit_handle_t s_{{prefix_lc}}_pcnt = NULL;

static void {{prefix_lc}}_poll_cb(void *arg)
{
    int count = 0;
    pcnt_unit_get_count(s_{{prefix_lc}}_pcnt, &count);
    if (count != 0) {
        pcnt_unit_clear_count(s_{{prefix_lc}}_pcnt);
        int8_t detents = (int8_t)(count / {{prefix}}_ENC_PER_DETENT);
        if (detents != 0) {{prefix_lc}}_on_step(detents);
    }
}
#else /* !SOC_PCNT_SUPPORTED — GPIO-ISR quadrature fallback (e.g. ESP32-C2/C3) */
/* Same contract as the PCNT path — a 50 ms poll hands accumulated detents to
 * {{prefix_lc}}_on_step() — only the counter is maintained by an ISR instead
 * of the pulse-counter peripheral.
 *
 * Decode: interrupt on the A-channel rising edge only; the B level sampled AT
 * that edge gives direction (valid for detented EC11-class encoders — the
 * detent sits where A toggles while B is stable). One accepted A edge = ONE
 * detent, so {{prefix}}_ENC_PER_DETENT does not apply on this path.
 *
 * Bounce: a re-trigger on the same mechanical edge re-reads the same B level
 * and would stack a same-direction count, so edges closer than 1 ms to the
 * accepted one are rejected. Real rotation is >=25 ms between A edges even
 * spinning fast, so no genuine detent is lost. */
static volatile int32_t s_{{prefix_lc}}_accum = 0;
static volatile int64_t s_{{prefix_lc}}_last_edge_us = INT64_MIN;
static portMUX_TYPE s_{{prefix_lc}}_mux = portMUX_INITIALIZER_UNLOCKED;

static void IRAM_ATTR {{prefix_lc}}_a_isr(void *arg)
{
    int64_t now = esp_timer_get_time();
    if (now - s_{{prefix_lc}}_last_edge_us < 1000) {
        return; /* < 1 ms after the accepted edge: contact bounce */
    }
    s_{{prefix_lc}}_last_edge_us = now;
    int dir = gpio_get_level((gpio_num_t){{prefix}}_ENC_B_GPIO) ? -1 : 1;
    portENTER_CRITICAL_ISR(&s_{{prefix_lc}}_mux);
    s_{{prefix_lc}}_accum += dir;
    portEXIT_CRITICAL_ISR(&s_{{prefix_lc}}_mux);
}

static void {{prefix_lc}}_poll_cb(void *arg)
{
    portENTER_CRITICAL(&s_{{prefix_lc}}_mux);
    int32_t detents = s_{{prefix_lc}}_accum;
    s_{{prefix_lc}}_accum = 0;
    portEXIT_CRITICAL(&s_{{prefix_lc}}_mux);
    if (detents != 0) {{prefix_lc}}_on_step((int8_t)detents);
}
#endif /* SOC_PCNT_SUPPORTED */
