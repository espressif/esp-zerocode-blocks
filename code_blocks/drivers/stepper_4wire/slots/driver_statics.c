static const uint8_t s_{{prefix_lc}}_seq[8][4] = {
    {1,0,0,0}, {1,1,0,0}, {0,1,0,0}, {0,1,1,0},
    {0,0,1,0}, {0,0,1,1}, {0,0,0,1}, {1,0,0,1},
};
static volatile int32_t s_{{prefix_lc}}_target = 0;
static int32_t s_{{prefix_lc}}_current = 0;
static int s_{{prefix_lc}}_phase = 0;

static void {{prefix_lc}}_step_apply(int phase)
{
    gpio_set_level((gpio_num_t){{prefix}}_ST_IN1, s_{{prefix_lc}}_seq[phase][0]);
    gpio_set_level((gpio_num_t){{prefix}}_ST_IN2, s_{{prefix_lc}}_seq[phase][1]);
    gpio_set_level((gpio_num_t){{prefix}}_ST_IN3, s_{{prefix_lc}}_seq[phase][2]);
    gpio_set_level((gpio_num_t){{prefix}}_ST_IN4, s_{{prefix_lc}}_seq[phase][3]);
}

static void {{prefix_lc}}_step_task(void *arg)
{
    for (;;) {
        int32_t target = s_{{prefix_lc}}_target;
        if (target == s_{{prefix_lc}}_current) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        int dir = (target > s_{{prefix_lc}}_current) ? 1 : -1;
        s_{{prefix_lc}}_phase = (s_{{prefix_lc}}_phase + dir + 8) & 7;
        {{prefix_lc}}_step_apply(s_{{prefix_lc}}_phase);
        s_{{prefix_lc}}_current += dir;
        esp_rom_delay_us({{prefix}}_ST_DELAY_US);
    }
}
