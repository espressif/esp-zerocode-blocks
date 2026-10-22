{
    esp_matter::endpoint::mounted_dimmable_load_control::config_t {{prefix_lc}}_cfg;
    /* esp_matter defaults StartUpCurrentLevel to 0, which the spec defines as
       "MinLevel at power-up": the load came back at level 1 and looked off. Null
       restores the previous level; the initial level matches the drivers'
       (same fix as dimmable_light). */
    {{prefix_lc}}_cfg.level_control.current_level = 254;
    {{prefix_lc}}_cfg.level_control_lighting.start_up_current_level = nullable<uint8_t>();
    esp_matter::endpoint_t *ep = esp_matter::endpoint::mounted_dimmable_load_control::create(
        node, &{{prefix_lc}}_cfg, ENDPOINT_FLAG_NONE, NULL);
    if (!ep) { ESP_LOGE(TAG, "Failed to create {{prefix_lc}} endpoint"); return ESP_FAIL; }
    s_{{prefix_lc}}_endpoint_id = esp_matter::endpoint::get_id(ep);
    ESP_LOGI(TAG, "{{prefix_lc}}: mounted_dimmable_load_control endpoint id=%d", s_{{prefix_lc}}_endpoint_id);
    /* NVS wear: CurrentLevel changes on every dimming step — defer its
       persistence (esp-matter examples do the same). */
    {
        esp_matter::attribute_t *a = esp_matter::attribute::get(
            s_{{prefix_lc}}_endpoint_id, chip::app::Clusters::LevelControl::Id,
            chip::app::Clusters::LevelControl::Attributes::CurrentLevel::Id);
        if (a) esp_matter::attribute::set_deferred_persistence(a);
    }
}
