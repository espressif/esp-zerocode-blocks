{
    // CCT is driven via the app_driver (param {{cfg.color_temp_param}}, mireds) — NOT
    // through the cluster config. Do NOT add fields like
    // `color_control.color_temperature_mireds` / `.color_temperature` to this
    // config_t: they DO NOT EXIST in esp_matter 1.4.2 and will not compile.
    // The defaults below are correct; set the device's initial CCT in the driver.
    esp_matter::endpoint::color_temperature_light::config_t {{prefix_lc}}_cfg;
    /* esp_matter defaults StartUpCurrentLevel to 0, which the spec defines as
       "MinLevel at power-up": the light booted at level 1 and looked off. Null
       restores the previous level; the initial level matches the drivers'. */
    {{prefix_lc}}_cfg.level_control.current_level = 254;
    {{prefix_lc}}_cfg.level_control_lighting.start_up_current_level = nullable<uint8_t>();
    esp_matter::endpoint_t *ep = esp_matter::endpoint::color_temperature_light::create(
        node, &{{prefix_lc}}_cfg, ENDPOINT_FLAG_NONE, NULL);
    if (!ep) { ESP_LOGE(TAG, "Failed to create {{prefix_lc}} endpoint"); return ESP_FAIL; }
    s_{{prefix_lc}}_endpoint_id = esp_matter::endpoint::get_id(ep);
    ESP_LOGI(TAG, "{{prefix_lc}}: color_temperature_light endpoint id=%d", s_{{prefix_lc}}_endpoint_id);
    /* NVS wear: CurrentLevel changes on every dimming step — defer its
       persistence (esp-matter examples do the same). */
    {
        esp_matter::attribute_t *a = esp_matter::attribute::get(
            s_{{prefix_lc}}_endpoint_id, chip::app::Clusters::LevelControl::Id,
            chip::app::Clusters::LevelControl::Attributes::CurrentLevel::Id);
        if (a) esp_matter::attribute::set_deferred_persistence(a);
    }
    /* Same for the color-temperature sweep attribute. */
    {
        esp_matter::attribute_t *a = esp_matter::attribute::get(
            s_{{prefix_lc}}_endpoint_id, chip::app::Clusters::ColorControl::Id,
            chip::app::Clusters::ColorControl::Attributes::ColorTemperatureMireds::Id);
        if (a) esp_matter::attribute::set_deferred_persistence(a);
    }
}
