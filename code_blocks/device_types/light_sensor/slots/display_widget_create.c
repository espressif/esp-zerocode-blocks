{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *name = lv_label_create(card);
    lv_label_set_text(name, "{{cfg.label}}");
    s_{{prefix_lc}}_val_label = lv_label_create(card);
    {
        app_driver_param_val_t init = {};
        app_driver_get_param({{cfg.value_param}}, &init);
        app_driver_param_val_t val = init;
        lv_label_set_text_fmt(s_{{prefix_lc}}_val_label, "%u", (unsigned)val.u16);
    }
}
