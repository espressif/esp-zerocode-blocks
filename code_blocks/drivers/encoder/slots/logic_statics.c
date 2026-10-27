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

/* Counts read off the unit but not yet a whole detent. The poll clears the
   hardware counter every time it sees counts, so without this a click split
   across two polls (or a part with fewer counts per click than
   pulses_per_detent assumes) was simply thrown away. */
static int s_{{prefix_lc}}_rem = 0;

static void {{prefix_lc}}_poll_cb(void *arg)
{
    int count = 0;
    pcnt_unit_get_count(s_{{prefix_lc}}_pcnt, &count);
    if (count == 0) return;
    pcnt_unit_clear_count(s_{{prefix_lc}}_pcnt);
    int total = s_{{prefix_lc}}_rem + count;
    int detents = total / {{prefix}}_ENC_PER_DETENT;
    /* on_step takes an int8; a burst past that stays in the remainder and is
       delivered on the next poll rather than wrapping the sign. */
    if (detents > 127) detents = 127;
    if (detents < -127) detents = -127;
    s_{{prefix_lc}}_rem = total - detents * {{prefix}}_ENC_PER_DETENT;
    if (detents != 0) {{prefix_lc}}_on_step((int8_t)detents);
}
#else /* !SOC_PCNT_SUPPORTED — GPIO-ISR quadrature fallback (e.g. ESP32-C2/C3) */
/* Same contract as the PCNT path — a 50 ms poll hands accumulated detents to
 * {{prefix_lc}}_on_step() — only the counter is kept by an ISR instead of the
 * pulse-counter peripheral.
 *
 * Decode: a full x4 quadrature state machine. Both channels interrupt on ANY
 * edge; the ISR reads A and B and looks the (previous, current) state pair up
 * in the Gray-code table: a valid neighbour moves the count by one quarter
 * step, an unchanged or impossible pair (a bounce that skipped a state) moves
 * it by nothing. Contact bounce on one channel therefore toggles between two
 * neighbouring states and cancels itself out — no time window, so no fast
 * spin is dropped. Quarter steps become detents exactly as on the PCNT path:
 * {{prefix}}_ENC_PER_DETENT counts per click, the remainder carried over. */
static volatile int32_t s_{{prefix_lc}}_accum = 0;
static volatile uint8_t s_{{prefix_lc}}_state = 0;
static int s_{{prefix_lc}}_rem = 0;
static portMUX_TYPE s_{{prefix_lc}}_mux = portMUX_INITIALIZER_UNLOCKED;

/* Index: (previous AB << 2) | current AB. +1 = A leads B, -1 = B leads A. */
static const int8_t s_{{prefix_lc}}_qdec[16] = {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0,
};

static void IRAM_ATTR {{prefix_lc}}_ab_isr(void *arg)
{
    uint8_t cur = (uint8_t)((gpio_get_level((gpio_num_t){{prefix}}_ENC_A_GPIO) << 1) |
                            gpio_get_level((gpio_num_t){{prefix}}_ENC_B_GPIO));
    portENTER_CRITICAL_ISR(&s_{{prefix_lc}}_mux);
    s_{{prefix_lc}}_accum += s_{{prefix_lc}}_qdec[(s_{{prefix_lc}}_state << 2) | cur];
    s_{{prefix_lc}}_state = cur;
    portEXIT_CRITICAL_ISR(&s_{{prefix_lc}}_mux);
}

static void {{prefix_lc}}_poll_cb(void *arg)
{
    portENTER_CRITICAL(&s_{{prefix_lc}}_mux);
    int32_t count = s_{{prefix_lc}}_accum;
    s_{{prefix_lc}}_accum = 0;
    portEXIT_CRITICAL(&s_{{prefix_lc}}_mux);
    if (count == 0) return;
    int total = s_{{prefix_lc}}_rem + (int)count;
    int detents = total / {{prefix}}_ENC_PER_DETENT;
    if (detents > 127) detents = 127;
    if (detents < -127) detents = -127;
    s_{{prefix_lc}}_rem = total - detents * {{prefix}}_ENC_PER_DETENT;
    if (detents != 0) {{prefix_lc}}_on_step((int8_t)detents);
}
#endif /* SOC_PCNT_SUPPORTED */
