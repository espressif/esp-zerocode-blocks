{
    esp_matter::endpoint::extended_color_light::config_t {{prefix_lc}}_cfg;
    /* esp_matter defaults StartUpCurrentLevel to 0, which the spec defines as
       "MinLevel at power-up": the light booted at level 1 and looked off. Null
       restores the previous level; the initial level matches the drivers'. */
    {{prefix_lc}}_cfg.level_control.current_level = 254;
    {{prefix_lc}}_cfg.level_control_lighting.start_up_current_level = nullable<uint8_t>();
    /* Controllers draw their white slider from the physical range; esp_matter's
       default 1..65279 mireds is meaningless. 2000..6500 K, the usual span. */
    {{prefix_lc}}_cfg.color_control_color_temperature.color_temp_physical_min_mireds = 153;
    {{prefix_lc}}_cfg.color_control_color_temperature.color_temp_physical_max_mireds = 500;
    {{prefix_lc}}_cfg.color_control_color_temperature.couple_color_temp_to_level_min_mireds = 153;
    esp_matter::endpoint_t *ep = esp_matter::endpoint::extended_color_light::create(
        node, &{{prefix_lc}}_cfg, ENDPOINT_FLAG_NONE, NULL);
    if (!ep) { ESP_LOGE(TAG, "Failed to create {{prefix_lc}} endpoint"); return ESP_FAIL; }
    s_{{prefix_lc}}_endpoint_id = esp_matter::endpoint::get_id(ep);
    ESP_LOGI(TAG, "{{prefix_lc}}: extended_color_light endpoint id=%d", s_{{prefix_lc}}_endpoint_id);
    /* For Extended Color Light the spec makes XY and CT mandatory and HS
       optional, and esp_matter enables only the mandatory pair. This block
       drives hue/saturation, so add HS — otherwise CurrentHue/CurrentSaturation
       never exist and every write to them fails. */
    {
        esp_matter::cluster_t *cc = esp_matter::cluster::get(ep, chip::app::Clusters::ColorControl::Id);
        esp_matter::cluster::color_control::feature::hue_saturation::config_t hs_cfg;
        esp_err_t err = cc ? esp_matter::cluster::color_control::feature::hue_saturation::add(cc, &hs_cfg)
                           : ESP_ERR_NOT_FOUND;
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "{{prefix_lc}}: hue_saturation feature: %s", esp_err_to_name(err));
            return ESP_FAIL;
        }
    }
    /* NVS wear: CurrentLevel changes on every dimming step — defer its
       persistence (esp-matter examples do the same). */
    {
        esp_matter::attribute_t *a = esp_matter::attribute::get(
            s_{{prefix_lc}}_endpoint_id, chip::app::Clusters::LevelControl::Id,
            chip::app::Clusters::LevelControl::Attributes::CurrentLevel::Id);
        if (a) esp_matter::attribute::set_deferred_persistence(a);
    }
    /* Same for the color-sweep attributes. */
    {
        static const uint32_t attrs[] = {
            chip::app::Clusters::ColorControl::Attributes::CurrentHue::Id,
            chip::app::Clusters::ColorControl::Attributes::CurrentSaturation::Id,
            /* controllers may drive XY even though our driver speaks hue/sat */
            chip::app::Clusters::ColorControl::Attributes::CurrentX::Id,
            chip::app::Clusters::ColorControl::Attributes::CurrentY::Id,
            chip::app::Clusters::ColorControl::Attributes::ColorTemperatureMireds::Id,
        };
        for (uint32_t id : attrs) {
            esp_matter::attribute_t *a = esp_matter::attribute::get(
                s_{{prefix_lc}}_endpoint_id, chip::app::Clusters::ColorControl::Id, id);
            if (a) esp_matter::attribute::set_deferred_persistence(a);
        }
    }
}
