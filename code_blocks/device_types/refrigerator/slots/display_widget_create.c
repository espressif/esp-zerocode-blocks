{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *name = lv_label_create(card);
    lv_label_set_text(name, "{{cfg.label}}");
    s_{{prefix_lc}}_mode_dd = lv_dropdown_create(card);
    lv_dropdown_set_options(s_{{prefix_lc}}_mode_dd, "Normal\nEnergy save\nRapid cool");
    {
        app_driver_param_val_t init = {};
        app_driver_get_param({{cfg.mode_param}}, &init);
        if (init.u8 < 3) lv_dropdown_set_selected(s_{{prefix_lc}}_mode_dd, init.u8);
    }
    lv_obj_add_event_cb(s_{{prefix_lc}}_mode_dd, {{prefix_lc}}_mode_dd_cb, LV_EVENT_VALUE_CHANGED, NULL);
}
