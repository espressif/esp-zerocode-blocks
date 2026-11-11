static lv_obj_t *s_{{prefix_lc}}_val_label = NULL;

/* LVGL's own printf has no %f by default: format here. */
static void {{prefix_lc}}_display_show(int16_t raw)
{
    const int scale = {{cfg.scale}};
    const int dp = scale >= 1000 ? 3 : scale >= 100 ? 2 : scale >= 10 ? 1 : 0;
    char buf[32];
    snprintf(buf, sizeof(buf), "%.*f %s", dp, (double)raw / scale, "{{cfg.unit}}");
    lv_label_set_text(s_{{prefix_lc}}_val_label, buf);
}
