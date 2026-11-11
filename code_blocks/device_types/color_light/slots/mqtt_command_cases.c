if (tlen == (int)strlen(s_{{prefix_lc}}_cmd_topic) &&
    strncmp(topic, s_{{prefix_lc}}_cmd_topic, tlen) == 0) {
    /* JSON schema command: {"state":"ON","brightness":128,"color":{"h":240,"s":80}} —
     * every key optional. Read in place with json_parser: the payload is not
     * NUL-terminated and json_parse_start takes its length. Each accessor
     * answers OS_SUCCESS only when the member exists with the requested type,
     * so a missing key is simply skipped. HA may send h/s as 240 or 240.0 —
     * floats accept both; brightness is always an integer. */
    jparse_ctx_t jctx;
    if (json_parse_start(&jctx, data, dlen) != OS_SUCCESS) {
        ESP_LOGW(TAG, "{{prefix_lc}}: unparseable command payload");
        return;
    }
    char state[8];
    if (json_obj_get_string(&jctx, "state", state, sizeof(state)) == OS_SUCCESS) {
        app_driver_param_val_t v = { .b = (strcmp(state, "ON") == 0) };
        app_driver_set_param({{cfg.power_param}}, v, s_handle);
    }
    int b = 0;
    if (json_obj_get_int(&jctx, "brightness", &b) == OS_SUCCESS) {
        if (b < 1) b = 1;
        if (b > 254) b = 254;
        app_driver_param_val_t v = { .u8 = (uint8_t)b };
        app_driver_set_param({{cfg.brightness_param}}, v, s_handle);
    }
    if (json_obj_get_object(&jctx, "color") == OS_SUCCESS) {
        float hf = 0, sf = 0;
        if (json_obj_get_float(&jctx, "h", &hf) == OS_SUCCESS) {
            int h = (int)((hf * 254.0f) / 360.0f + 0.5f);
            if (h < 0) h = 0;
            if (h > 254) h = 254;
            app_driver_param_val_t v = { .u8 = (uint8_t)h };
            app_driver_set_param({{cfg.hue_param}}, v, s_handle);
        }
        if (json_obj_get_float(&jctx, "s", &sf) == OS_SUCCESS) {
            int s = (int)((sf * 254.0f) / 100.0f + 0.5f);
            if (s < 0) s = 0;
            if (s > 254) s = 254;
            app_driver_param_val_t v = { .u8 = (uint8_t)s };
            app_driver_set_param({{cfg.saturation_param}}, v, s_handle);
        }
        json_obj_leave_object(&jctx);
    }
    json_parse_end(&jctx);
    s_{{prefix_lc}}_publish_state();
    return;
}
