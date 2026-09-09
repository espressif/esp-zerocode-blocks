static uint16_t s_{{prefix_lc}}_target = 0;

static void {{prefix_lc}}_motor_off(void)
{
    gpio_set_level((gpio_num_t){{prefix}}_UP_GPIO, 0);
    gpio_set_level((gpio_num_t){{prefix}}_DOWN_GPIO, 0);
}
