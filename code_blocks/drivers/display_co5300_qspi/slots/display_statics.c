/* The CO5300 only takes draw windows on a 2-pixel grid; an odd-edged LVGL area
 * renders slanted. Why: EVEN_AREA_PANELS in engine/src/generator.ts. */
static void {{prefix_lc}}_round_to_even(lv_area_t *area)
{
    area->x1 &= ~1;
    area->x2 |= 1;
    area->y1 &= ~1;
    area->y2 |= 1;
}
