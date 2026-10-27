if (device == s_{{prefix_lc}}_rmp_device && param == s_{{prefix_lc}}_rmp_param) {
    app_driver_param_val_t pv = {};
#if defined({{prefix}}_RMP_KIND_time)
    unsigned hh = 0, mm = 0;
    if (val.type != RMAKER_VAL_TYPE_STRING || !val.val.s || sscanf(val.val.s, "%u:%u", &hh, &mm) != 2 || hh > 23 || mm > 59) {
        ESP_LOGW(TAG, "{{prefix_lc}}: '%s' is not a time (HH:MM)", (val.type == RMAKER_VAL_TYPE_STRING && val.val.s) ? val.val.s : "?");
        return ESP_ERR_INVALID_ARG;
    }
    pv.u16 = (uint16_t)(hh * 60U + mm);
#elif defined({{prefix}}_RMP_VT_bool)
    pv.b = val.val.b;
#else
    int n = val.val.i;
#if !defined({{prefix}}_RMP_KIND_toggle)
    if (n < {{cfg.min}}) n = {{cfg.min}};
    if (n > {{cfg.max}}) n = {{cfg.max}};
#endif
#if defined({{prefix}}_RMP_VT_u8)
    pv.u8 = (uint8_t)n;
#elif defined({{prefix}}_RMP_VT_i16)
    pv.i16 = (int16_t)n;
#elif defined({{prefix}}_RMP_VT_u16)
    pv.u16 = (uint16_t)n;
#else
    pv.u32 = (uint32_t)n;
#endif
#endif
    app_driver_set_param({{cfg.param}}, pv, s_handle);
    esp_rmaker_param_update_and_report(param, {{prefix_lc}}_rmp_val(pv));
    return ESP_OK;
}
