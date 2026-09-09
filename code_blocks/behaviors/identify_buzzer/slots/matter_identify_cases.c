if (type == esp_matter::identification::callback_type_t::START) {
    /* fire_event, not set_param: the melody is one-shot, so a second Identify
     * with the pattern value unchanged would be deduped into silence. */
    app_driver_param_val_t v = { .u8 = {{prefix}}_BUZZER_IDENTIFY_PATTERN };
    app_driver_fire_event({{cfg.pattern_param}}, v, APP_DRIVER_SOURCE_LOCAL);
}
