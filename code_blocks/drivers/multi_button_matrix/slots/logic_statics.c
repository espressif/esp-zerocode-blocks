extern const int s_{{prefix_lc}}_rows[{{prefix}}_MATRIX_ROWS];
extern const int s_{{prefix_lc}}_cols[{{prefix}}_MATRIX_COLS];
static uint8_t s_{{prefix_lc}}_last_key = 0xFF;

static void {{prefix_lc}}_matrix_scan_cb(void *arg)
{
    uint8_t pressed = 0xFF;
    for (int r = 0; r < {{prefix}}_MATRIX_ROWS && pressed == 0xFF; ++r) {
        gpio_set_level((gpio_num_t)s_{{prefix_lc}}_rows[r], 0);
        esp_rom_delay_us(5);
        for (int c = 0; c < {{prefix}}_MATRIX_COLS; ++c) {
            if (gpio_get_level((gpio_num_t)s_{{prefix_lc}}_cols[c]) == 0) {
                pressed = (uint8_t)(r * {{prefix}}_MATRIX_COLS + c);
                break;
            }
        }
        gpio_set_level((gpio_num_t)s_{{prefix_lc}}_rows[r], 1);
    }
    if (pressed != s_{{prefix_lc}}_last_key) {
        s_{{prefix_lc}}_last_key = pressed;
        app_driver_param_val_t v = { .u8 = pressed };
        app_driver_set_param({{cfg.index_param}}, v, APP_DRIVER_SOURCE_LOCAL);
    }
}
