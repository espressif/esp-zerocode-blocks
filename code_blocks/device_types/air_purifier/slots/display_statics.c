static lv_obj_t *s_{{prefix_lc}}_spd_slider = NULL;

/* LVGL task — no lock. */
static void {{prefix_lc}}_spd_slider_cb(lv_event_t *e)
{
    lv_obj_t *s = (lv_obj_t *)lv_event_get_target(e);
    int32_t v = lv_slider_get_value(s);
    app_driver_param_val_t pv = { .u8 = (uint8_t)(v) };
    app_driver_set_param({{cfg.speed_param}}, pv, s_handle);
}
