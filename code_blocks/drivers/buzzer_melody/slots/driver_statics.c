/* Catch a channel/timer the target's LEDC does not have at compile time —
   ledc_channel_config() would otherwise return ESP_ERR_INVALID_ARG under
   ESP_ERROR_CHECK and the device would boot-loop (channels 6-7 exist only on
   the chips with 8). */
static_assert({{prefix}}_MEL_CHANNEL >= 0 && {{prefix}}_MEL_CHANNEL < LEDC_CHANNEL_MAX,
              "{{prefix_lc}}: ledc_channel does not exist on this chip");
static_assert({{prefix}}_MEL_TIMER >= 0 && {{prefix}}_MEL_TIMER < LEDC_TIMER_MAX,
              "{{prefix_lc}}: ledc_timer does not exist on this chip");

static volatile uint8_t s_{{prefix_lc}}_pattern = 0;
static TaskHandle_t s_{{prefix_lc}}_task = NULL;

typedef struct { uint16_t freq; uint16_t ms; } {{prefix_lc}}_note_t;
static const {{prefix_lc}}_note_t s_{{prefix_lc}}_chirp[]    = { {2500,80}, {0,0} };
static const {{prefix_lc}}_note_t s_{{prefix_lc}}_three[]    = { {2500,80},{0,80}, {2500,80},{0,80}, {2500,80}, {0,0} };
static const {{prefix_lc}}_note_t s_{{prefix_lc}}_alarm[]    = { {2200,500},{0,500} };
static const {{prefix_lc}}_note_t s_{{prefix_lc}}_chime_up[] = { {880,150},{1320,150},{1760,250}, {0,0} };
static const {{prefix_lc}}_note_t s_{{prefix_lc}}_chime_dn[] = { {1760,150},{1320,150},{880,250},  {0,0} };

static void {{prefix_lc}}_play_note(uint16_t freq, uint16_t ms)
{
#if defined({{prefix}}_MEL_KIND_active)
    /* Active buzzer: the part sets the pitch; a note is just "on" for its length. */
    gpio_set_level((gpio_num_t){{prefix}}_MEL_GPIO,
                   freq != 0 ? {{prefix}}_MEL_ACTIVE_LEVEL : !{{prefix}}_MEL_ACTIVE_LEVEL);
#else
    if (freq == 0) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t){{prefix}}_MEL_CHANNEL, 0);
    } else {
        ledc_set_freq(LEDC_LOW_SPEED_MODE, (ledc_timer_t){{prefix}}_MEL_TIMER, freq);
        /* 50 % duty at the timer's resolution. */
        ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t){{prefix}}_MEL_CHANNEL,
                      (1u << {{prefix}}_MEL_DUTY_BITS) / 2);
    }
    ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t){{prefix}}_MEL_CHANNEL);
#endif
    if (ms > 0) vTaskDelay(pdMS_TO_TICKS(ms));
}

static const {{prefix_lc}}_note_t *{{prefix_lc}}_pattern_for(uint8_t p)
{
    switch (p) {
        case 1: return s_{{prefix_lc}}_chirp;
        case 2: return s_{{prefix_lc}}_three;
        case 3: return s_{{prefix_lc}}_alarm;
        case 4: return s_{{prefix_lc}}_chime_up;
        case 5: return s_{{prefix_lc}}_chime_dn;
        default: return NULL;
    }
}

static void {{prefix_lc}}_play_task(void *arg)
{
    for (;;) {
        uint8_t cur = s_{{prefix_lc}}_pattern;
        const {{prefix_lc}}_note_t *seq = {{prefix_lc}}_pattern_for(cur);
        if (!seq) {
            {{prefix_lc}}_play_note(0, 0);
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        for (const {{prefix_lc}}_note_t *n = seq; n->ms; ++n) {
            if (s_{{prefix_lc}}_pattern != cur) break;
            {{prefix_lc}}_play_note(n->freq, n->ms);
        }
        {{prefix_lc}}_play_note(0, 0);
        /* Looping pattern (alarm) repeats; a one-shot clears itself — ON THE
           BUS, not just here. Clearing only the local copy left the bus
           holding the pattern, so the next set_param of the same pattern was
           deduped (a "chirp on X" chirped once per boot) and get_param
           reported a pattern that was not playing. Compare-and-clear: a
           different pattern that arrived while this one played is kept. */
        if (cur != 3 && s_{{prefix_lc}}_pattern == cur) {
            app_driver_param_val_t zero = { .u8 = 0 };
            app_driver_set_param({{cfg.pattern_param}}, zero, APP_DRIVER_SOURCE_LOCAL);
        }
    }
}
