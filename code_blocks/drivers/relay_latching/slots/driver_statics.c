/* The coil pulse ends on a one-shot timer, so the command returns at once:
 * a vTaskDelay here held whichever task sent it (Matter, a timer, the
 * console) for the whole pulse. A new command ends a pulse still running
 * before it drives the other coil, so both coils are never energised. */
static esp_timer_handle_t s_{{prefix_lc}}_latch_timer = NULL;
static portMUX_TYPE s_{{prefix_lc}}_latch_mux = portMUX_INITIALIZER_UNLOCKED;
/* Pulses started, and pulses whose end has run or was cancelled: only the end
 * of the LATEST pulse idles the coils. A command can arrive while the previous
 * pulse's timer callback is already on its way (esp_timer_stop can no longer
 * cancel it); that stale end must not cut the new pulse short. */
static uint32_t s_{{prefix_lc}}_latch_started = 0;
static uint32_t s_{{prefix_lc}}_latch_done = 0;

static void {{prefix_lc}}_latch_coils_idle(void)
{
    gpio_set_level((gpio_num_t){{prefix}}_LATCH_SET_GPIO, !{{prefix}}_LATCH_LEVEL);
    gpio_set_level((gpio_num_t){{prefix}}_LATCH_RESET_GPIO, !{{prefix}}_LATCH_LEVEL);
}

static void {{prefix_lc}}_latch_end_cb(void *arg)
{
    (void)arg;
    portENTER_CRITICAL(&s_{{prefix_lc}}_latch_mux);
    s_{{prefix_lc}}_latch_done++;
    if (s_{{prefix_lc}}_latch_done == s_{{prefix_lc}}_latch_started) {{prefix_lc}}_latch_coils_idle();
    portEXIT_CRITICAL(&s_{{prefix_lc}}_latch_mux);
}

static void {{prefix_lc}}_latch_pulse(bool on)
{
    if (s_{{prefix_lc}}_latch_timer && esp_timer_stop(s_{{prefix_lc}}_latch_timer) == ESP_OK) {
        /* Cancelled before it ran: that pulse's end will never come. */
        portENTER_CRITICAL(&s_{{prefix_lc}}_latch_mux);
        s_{{prefix_lc}}_latch_done++;
        portEXIT_CRITICAL(&s_{{prefix_lc}}_latch_mux);
    }
    portENTER_CRITICAL(&s_{{prefix_lc}}_latch_mux);
    s_{{prefix_lc}}_latch_started++;
    {{prefix_lc}}_latch_coils_idle();
    gpio_set_level((gpio_num_t)(on ? {{prefix}}_LATCH_SET_GPIO : {{prefix}}_LATCH_RESET_GPIO), {{prefix}}_LATCH_LEVEL);
    portEXIT_CRITICAL(&s_{{prefix_lc}}_latch_mux);
    if (s_{{prefix_lc}}_latch_timer == NULL ||
        esp_timer_start_once(s_{{prefix_lc}}_latch_timer, (uint64_t){{prefix}}_LATCH_PULSE_MS * 1000ULL) != ESP_OK) {
        /* No timer: end the pulse here rather than leave a coil energised. */
        esp_rom_delay_us((uint32_t){{prefix}}_LATCH_PULSE_MS * 1000U);
        {{prefix_lc}}_latch_end_cb(NULL);
    }
}
