/* {{prefix_lc}}: the board's own '{{cfg.device}}' gpio_ctrl device.
 * The pin and the active level are read from the BOARD at init — no GPIO
 * number appears anywhere in this product, which is the point: the line
 * belongs to the board definition and claiming it here would be an IO
 * conflict with it. -1 until init resolves the handle, and every write is a
 * no-op until then, so a missing device degrades to "does nothing" rather
 * than driving GPIO -1. */
static int s_{{prefix_lc}}_gpio   = -1;
static int s_{{prefix_lc}}_active = 1;

static void {{prefix_lc}}_board_gpio_set(bool on)
{
    if (s_{{prefix_lc}}_gpio < 0) {
        return;
    }
    gpio_set_level((gpio_num_t)s_{{prefix_lc}}_gpio,
                   on ? s_{{prefix_lc}}_active : !s_{{prefix_lc}}_active);
}
