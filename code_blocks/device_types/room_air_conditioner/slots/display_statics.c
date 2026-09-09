static lv_obj_t *s_{{prefix_lc}}_pw_sw = NULL;
static lv_obj_t *s_{{prefix_lc}}_sp_slider = NULL;

/* LVGL task — no lock. */
static void {{prefix_lc}}_pw_sw_cb(lv_event_t *e)
{
    lv_obj_t *sw = (lv_obj_t *)lv_event_get_target(e);
    app_driver_param_val_t v = { .b = lv_obj_has_state(sw, LV_STATE_CHECKED) };
    app_driver_set_param({{cfg.power_param}}, v, s_handle);
}
/* LVGL task — no lock. */
static void {{prefix_lc}}_sp_slider_cb(lv_event_t *e)
{
    lv_obj_t *s = (lv_obj_t *)lv_event_get_target(e);
    int32_t v = lv_slider_get_value(s);
    app_driver_param_val_t pv = { .i16 = (int16_t)(v * 100) };
    app_driver_set_param({{cfg.cool_setpoint_param}}, pv, s_handle);
}
