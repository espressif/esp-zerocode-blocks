#define {{prefix}}_OLED_PORT  {{cfg.i2c_port}}
#define {{prefix}}_OLED_ADDR  {{cfg.i2c_address}}
#define {{prefix}}_OLED_FREQ_HZ  {{cfg.i2c_freq_hz}}

#ifdef __cplusplus
extern "C" {
#endif
/* drivers/oled_ssd1306 ({{prefix_lc}}): the drawing API, defined in app_driver.
 * Coordinates are pixels, x 0-127 left to right, y 0-63 top to bottom.
 * _text/_fill only change the frame; _show sends it to the panel once.
 * _print and _clear send it themselves. Safe from any task. */
void {{prefix_lc}}_oled_clear(void);
void {{prefix_lc}}_oled_print(int row, const char *s);
void {{prefix_lc}}_oled_text(int x, int y, int scale, const char *s);
void {{prefix_lc}}_oled_fill(int x, int y, int w, int h, int on);
void {{prefix_lc}}_oled_show(void);
#ifdef __cplusplus
}
#endif
