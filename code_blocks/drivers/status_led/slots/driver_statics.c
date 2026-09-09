static led_indicator_handle_t s_{{prefix_lc}}_led = NULL;

led_indicator_handle_t {{prefix_lc}}_status_led_handle(void) { return s_{{prefix_lc}}_led; }

/* ── SINGLE-OWNER LED CONTROL ──────────────────────────────────────────
   ALL status-LED effects route through this file. The led_indicator handle
   owns the GPIO; we blink it by toggling set_on_off() from a software timer,
   so there is exactly one owner and one code path. Never gpio_set_level()
   this pin from anywhere. */

static esp_timer_handle_t s_{{prefix_lc}}_blink_timer = NULL;
static volatile uint8_t   s_{{prefix_lc}}_pattern = 0;
static volatile bool      s_{{prefix_lc}}_blink_on = false;

static void {{prefix_lc}}_blink_timer_cb(void *arg) {
    if (!s_{{prefix_lc}}_led) return;
    uint8_t p = s_{{prefix_lc}}_pattern;
    if (p == 2 || p == 3) {
        /* toggle for slow/fast blink */
        s_{{prefix_lc}}_blink_on = !s_{{prefix_lc}}_blink_on;
        led_indicator_set_on_off(s_{{prefix_lc}}_led, s_{{prefix_lc}}_blink_on);
    } else if (p == 4) {
        /* coarse breathe via brightness ramp */
        static uint8_t step = 0;
        static const uint8_t ramp[] = { 20, 90, 180, 255, 180, 90, 20, 0 };
        led_indicator_set_brightness(s_{{prefix_lc}}_led, ramp[step]);
        step = (uint8_t)((step + 1) % (sizeof(ramp) / sizeof(ramp[0])));
    }
}

static void {{prefix_lc}}_blink_arm(uint32_t period_us) {
    if (!s_{{prefix_lc}}_blink_timer) return;
    esp_timer_stop(s_{{prefix_lc}}_blink_timer);
    if (period_us) {
        esp_timer_start_periodic(s_{{prefix_lc}}_blink_timer, period_us);
    }
}

/* Solid on/off for product-state feedback. Cancels any active pattern. */
void {{prefix_lc}}_status_led_set(bool on) {
    s_{{prefix_lc}}_pattern = on ? 1 : 0;
    {{prefix_lc}}_blink_arm(0);
    if (s_{{prefix_lc}}_led) led_indicator_set_on_off(s_{{prefix_lc}}_led, on);
}

/* THE single pattern code path. Every effect (commissioning, identify,
   blink_pattern) ends up here — via the LED_PATTERN apply case. */
void {{prefix_lc}}_status_led_set_pattern(uint8_t pattern) {
    if (!s_{{prefix_lc}}_led) return;
    s_{{prefix_lc}}_pattern = pattern;
    switch (pattern) {
        case 0: /* off  */ {{prefix_lc}}_blink_arm(0); led_indicator_set_on_off(s_{{prefix_lc}}_led, false); break;
        case 1: /* solid*/ {{prefix_lc}}_blink_arm(0); led_indicator_set_on_off(s_{{prefix_lc}}_led, true);  break;
        case 2: /* slow */ s_{{prefix_lc}}_blink_on = false; {{prefix_lc}}_blink_arm(500000); break;
        case 3: /* fast */ s_{{prefix_lc}}_blink_on = false; {{prefix_lc}}_blink_arm(125000); break;
        case 4: /* breathe */ {{prefix_lc}}_blink_arm(120000); break;
        default: {{prefix_lc}}_blink_arm(0); break;
    }
}
