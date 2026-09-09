static lv_obj_t *s_{{prefix_lc}}_mode_dd = NULL;

/* LVGL task — no lock. */
static void {{prefix_lc}}_mode_dd_cb(lv_event_t *e)
{
    lv_obj_t *dd = (lv_obj_t *)lv_event_get_target(e);
    app_driver_param_val_t v = { .u8 = (uint8_t)lv_dropdown_get_selected(dd) };
    app_driver_set_param({{cfg.mode_param}}, v, s_handle);
}
