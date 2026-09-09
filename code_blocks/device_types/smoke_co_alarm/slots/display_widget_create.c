{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *name = lv_label_create(card);
    lv_label_set_text(name, "{{cfg.label}}");
    s_{{prefix_lc}}_smoke_label = lv_label_create(card);
    {
        app_driver_param_val_t init = {};
        app_driver_get_param({{cfg.smoke_state_param}}, &init);
        lv_label_set_text(s_{{prefix_lc}}_smoke_label, (init.u8 != 0) ? "SMOKE!" : "Smoke OK");
    }
    s_{{prefix_lc}}_co_label = lv_label_create(card);
    {
        app_driver_param_val_t init = {};
        app_driver_get_param({{cfg.co_state_param}}, &init);
        lv_label_set_text(s_{{prefix_lc}}_co_label, (init.u8 != 0) ? "CO!" : "CO OK");
    }
}
