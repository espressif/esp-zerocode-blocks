{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *name = lv_label_create(card);
    lv_label_set_text(name, "{{cfg.label}}");
    s_{{prefix_lc}}_pw_sw = lv_switch_create(card);
    {
        app_driver_param_val_t init = {};
        app_driver_get_param({{cfg.power_param}}, &init);
        if (init.b) lv_obj_add_state(s_{{prefix_lc}}_pw_sw, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(s_{{prefix_lc}}_pw_sw, {{prefix_lc}}_pw_sw_cb, LV_EVENT_VALUE_CHANGED, NULL);
    s_{{prefix_lc}}_sp_slider = lv_slider_create(card);
    lv_obj_set_width(s_{{prefix_lc}}_sp_slider, lv_pct(90));
    lv_slider_set_range(s_{{prefix_lc}}_sp_slider, 30, 70);
    {
        app_driver_param_val_t init = {};
        app_driver_get_param({{cfg.setpoint_param}}, &init);
        app_driver_param_val_t val = init;
        lv_slider_set_value(s_{{prefix_lc}}_sp_slider, (int32_t)(val.i16 / 100), LV_ANIM_OFF);
    }
    lv_obj_add_event_cb(s_{{prefix_lc}}_sp_slider, {{prefix_lc}}_sp_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
}
