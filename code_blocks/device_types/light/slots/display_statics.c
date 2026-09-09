static lv_obj_t *s_{{prefix_lc}}_switch = NULL;

/* Runs ON the LVGL task — no lock. Toggling the card's switch drives the bus. */
static void {{prefix_lc}}_switch_event_cb(lv_event_t *e)
{
    lv_obj_t *sw = (lv_obj_t *)lv_event_get_target(e);
    app_driver_param_val_t v = { .b = lv_obj_has_state(sw, LV_STATE_CHECKED) };
    app_driver_set_param({{cfg.power_param}}, v, s_handle);
}
