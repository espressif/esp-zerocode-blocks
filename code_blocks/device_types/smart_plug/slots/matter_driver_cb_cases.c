if (param_id == {{cfg.power_param}}) {
    s_from_driver = true;
    esp_matter_attr_val_t mval = esp_matter_bool(val.b);
    esp_err_t err = esp_matter::attribute::update(
        s_{{prefix_lc}}_endpoint_id,
        chip::app::Clusters::OnOff::Id,
        chip::app::Clusters::OnOff::Attributes::OnOff::Id,
        &mval);
    if (err != ESP_OK) ESP_LOGE(TAG, "{{prefix_lc}}: update OnOff failed: %s", esp_err_to_name(err));
    s_from_driver = false;
}
