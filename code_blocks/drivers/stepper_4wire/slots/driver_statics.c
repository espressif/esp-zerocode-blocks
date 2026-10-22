static const uint8_t s_{{prefix_lc}}_seq[8][4] = {
    {1,0,0,0}, {1,1,0,0}, {0,1,0,0}, {0,1,1,0},
    {0,0,1,0}, {0,0,1,1}, {0,0,0,1}, {1,0,0,1},
};
static volatile int32_t s_{{prefix_lc}}_target = 0;
static int32_t s_{{prefix_lc}}_current = 0;
static int s_{{prefix_lc}}_phase = 0;
static esp_timer_handle_t s_{{prefix_lc}}_step_timer = NULL;

static void {{prefix_lc}}_step_apply(int phase)
{
    gpio_set_level((gpio_num_t){{prefix}}_ST_IN1, s_{{prefix_lc}}_seq[phase][0]);
    gpio_set_level((gpio_num_t){{prefix}}_ST_IN2, s_{{prefix_lc}}_seq[phase][1]);
    gpio_set_level((gpio_num_t){{prefix}}_ST_IN3, s_{{prefix_lc}}_seq[phase][2]);
    gpio_set_level((gpio_num_t){{prefix}}_ST_IN4, s_{{prefix_lc}}_seq[phase][3]);
}

static void {{prefix_lc}}_coils_off(void)
{
    gpio_set_level((gpio_num_t){{prefix}}_ST_IN1, 0);
    gpio_set_level((gpio_num_t){{prefix}}_ST_IN2, 0);
    gpio_set_level((gpio_num_t){{prefix}}_ST_IN3, 0);
    gpio_set_level((gpio_num_t){{prefix}}_ST_IN4, 0);
}

/* ONE half-step per tick of a periodic esp_timer, running only while the
 * motor moves. It used to be a priority-5 task that busy-waited
 * esp_rom_delay_us() between steps and never yielded: a 28BYJ-48 revolution
 * (~4096 half-steps, ~6 s) starved IDLE and every lower task on a single-core
 * chip, tripping the task watchdog. A task delay cannot pace it instead — one
 * tick is 10 ms at the default 100 Hz, ~7x the step time. A step is four
 * register writes, so the shared esp_timer task barely notices it. */
static void {{prefix_lc}}_step_cb(void *arg)
{
    (void)arg;
    int32_t target = s_{{prefix_lc}}_target;
    if (target == s_{{prefix_lc}}_current) {
        /* Arrived: stop, and DE-ENERGIZE — a held half-step is ~240 mA of
           continuous coil heat; the gearbox holds the shaft. */
        esp_timer_stop(s_{{prefix_lc}}_step_timer);
        {{prefix_lc}}_coils_off();
        /* A new target written between the read above and the stop found the
           timer still running, so its start was refused — restart for it. */
        if (s_{{prefix_lc}}_target != s_{{prefix_lc}}_current) {
            esp_timer_start_periodic(s_{{prefix_lc}}_step_timer, (uint64_t){{prefix}}_ST_DELAY_US);
        }
        return;
    }
    int dir = (target > s_{{prefix_lc}}_current) ? 1 : -1;
    s_{{prefix_lc}}_phase = (s_{{prefix_lc}}_phase + dir + 8) & 7;
    {{prefix_lc}}_step_apply(s_{{prefix_lc}}_phase);
    s_{{prefix_lc}}_current += dir;
}
