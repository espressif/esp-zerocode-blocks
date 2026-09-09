{
    esp_matter::endpoint::dimmable_plug_in_unit::config_t {{prefix_lc}}_cfg;
    esp_matter::endpoint_t *ep = esp_matter::endpoint::dimmable_plug_in_unit::create(
        node, &{{prefix_lc}}_cfg, ENDPOINT_FLAG_NONE, NULL);
    if (!ep) { ESP_LOGE(TAG, "Failed to create {{prefix_lc}} endpoint"); return ESP_FAIL; }
    s_{{prefix_lc}}_endpoint_id = esp_matter::endpoint::get_id(ep);
    ESP_LOGI(TAG, "{{prefix_lc}}: dimmable_plug_in_unit endpoint id=%d", s_{{prefix_lc}}_endpoint_id);
    /* NVS wear: CurrentLevel changes on every dimming step — defer its
       persistence (esp-matter examples do the same). */
    {
        esp_matter::attribute_t *a = esp_matter::attribute::get(
            s_{{prefix_lc}}_endpoint_id, chip::app::Clusters::LevelControl::Id,
            chip::app::Clusters::LevelControl::Attributes::CurrentLevel::Id);
        if (a) esp_matter::attribute::set_deferred_persistence(a);
    }
}
