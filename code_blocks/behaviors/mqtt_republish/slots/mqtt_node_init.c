{
    if ({{cfg.only_on_schedule}}) {
        app_mqtt_set_republish_only_on_schedule(true);
    }
    esp_err_t {{prefix_lc}}_rp_err = app_mqtt_set_republish_s((uint32_t){{cfg.republish_s}});
    if ({{prefix_lc}}_rp_err != ESP_OK) {
        ESP_LOGE(TAG, "republish every {{cfg.republish_s}} s not started (%s)", esp_err_to_name({{prefix_lc}}_rp_err));
    } else if ({{cfg.republish_s}} > 0) {
        ESP_LOGI(TAG, "republishing every entity's state every {{cfg.republish_s}} s%s",
                 ({{cfg.only_on_schedule}}) ? ", changes held for the schedule" : "");
    }
}
