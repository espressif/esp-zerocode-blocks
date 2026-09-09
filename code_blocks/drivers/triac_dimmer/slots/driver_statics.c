static volatile bool s_{{prefix_lc}}_on = false;
static volatile uint8_t s_{{prefix_lc}}_brightness = 254;
static esp_timer_handle_t s_{{prefix_lc}}_gate_timer = NULL;

static void {{prefix_lc}}_gate_fire(void *arg)
{
    if (!s_{{prefix_lc}}_on || s_{{prefix_lc}}_brightness == 0) return;
    gpio_set_level((gpio_num_t){{prefix}}_TRIAC_GATE_GPIO, 1);
    esp_rom_delay_us(20);
    gpio_set_level((gpio_num_t){{prefix}}_TRIAC_GATE_GPIO, 0);
}

static void IRAM_ATTR {{prefix_lc}}_zcd_isr(void *arg)
{
    if (!s_{{prefix_lc}}_on) return;
    uint32_t delay_us = ((uint32_t)({{prefix}}_TRIAC_HALF_US - 200) * (uint32_t)(255 - s_{{prefix_lc}}_brightness)) / 255U;
    if (delay_us < 100) delay_us = 100;
    esp_timer_stop(s_{{prefix_lc}}_gate_timer);
    esp_timer_start_once(s_{{prefix_lc}}_gate_timer, delay_us);
}
