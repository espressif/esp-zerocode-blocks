if (tlen == (int)strlen(s_{{prefix_lc}}_cmd_topic) &&
    strncmp(topic, s_{{prefix_lc}}_cmd_topic, tlen) == 0) {
    /* JSON schema command: {"state":"ON","brightness":128,"color":{"h":240,"s":80}} —
     * every key optional. cJSON ships with IDF. */
    cJSON *root = cJSON_ParseWithLength(data, dlen);
    if (!root) { ESP_LOGW(TAG, "{{prefix_lc}}: unparseable command payload"); return; }
    const cJSON *jstate = cJSON_GetObjectItemCaseSensitive(root, "state");
    if (cJSON_IsString(jstate)) {
        app_driver_param_val_t v = { .b = (strcmp(jstate->valuestring, "ON") == 0) };
        app_driver_set_param({{cfg.power_param}}, v, s_handle);
    }
    const cJSON *jbri = cJSON_GetObjectItemCaseSensitive(root, "brightness");
    if (cJSON_IsNumber(jbri)) {
        int b = jbri->valueint;
        if (b < 1) b = 1;
        if (b > 254) b = 254;
        app_driver_param_val_t v = { .u8 = (uint8_t)b };
        app_driver_set_param({{cfg.brightness_param}}, v, s_handle);
    }
    const cJSON *jcolor = cJSON_GetObjectItemCaseSensitive(root, "color");
    if (cJSON_IsObject(jcolor)) {
        const cJSON *jh = cJSON_GetObjectItemCaseSensitive(jcolor, "h");
        const cJSON *js = cJSON_GetObjectItemCaseSensitive(jcolor, "s");
        if (cJSON_IsNumber(jh)) {
            int h = (int)((jh->valuedouble * 254.0) / 360.0 + 0.5);
            if (h < 0) h = 0;
            if (h > 254) h = 254;
            app_driver_param_val_t v = { .u8 = (uint8_t)h };
            app_driver_set_param({{cfg.hue_param}}, v, s_handle);
        }
        if (cJSON_IsNumber(js)) {
            int s = (int)((js->valuedouble * 254.0) / 100.0 + 0.5);
            if (s < 0) s = 0;
            if (s > 254) s = 254;
            app_driver_param_val_t v = { .u8 = (uint8_t)s };
            app_driver_set_param({{cfg.saturation_param}}, v, s_handle);
        }
    }
    cJSON_Delete(root);
    s_{{prefix_lc}}_publish_state();
    return;
}
