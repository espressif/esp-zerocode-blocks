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
    if (freq == 0) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t){{prefix}}_MEL_CHANNEL, 0);
    } else {
        ledc_set_freq(LEDC_LOW_SPEED_MODE, (ledc_timer_t){{prefix}}_MEL_TIMER, freq);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t){{prefix}}_MEL_CHANNEL, 128);
    }
    ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t){{prefix}}_MEL_CHANNEL);
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
        /* Looping pattern (alarm) repeats; one-shot patterns reset to 0 */
        if (cur != 3) s_{{prefix_lc}}_pattern = 0;
    }
}
