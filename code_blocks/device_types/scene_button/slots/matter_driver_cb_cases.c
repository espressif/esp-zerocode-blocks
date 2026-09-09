if (param_id == {{cfg.position_param}}) {
    s_from_driver = true;
    esp_matter_attr_val_t mval = esp_matter_uint8(val.u8);
    esp_matter::attribute::update(s_{{prefix_lc}}_endpoint_id,
        chip::app::Clusters::Switch::Id,
        chip::app::Clusters::Switch::Attributes::CurrentPosition::Id, &mval);
    s_from_driver = false;
}
