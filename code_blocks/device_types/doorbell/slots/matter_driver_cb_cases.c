if (param_id == {{cfg.press_param}}) {
    s_from_driver = true;
    esp_matter_attr_val_t mval = esp_matter_uint8(val.b ? 1 : 0);
    esp_matter::attribute::update(s_{{prefix_lc}}_endpoint_id,
        chip::app::Clusters::Switch::Id,
        chip::app::Clusters::Switch::Attributes::CurrentPosition::Id, &mval);
    s_from_driver = false;
}
