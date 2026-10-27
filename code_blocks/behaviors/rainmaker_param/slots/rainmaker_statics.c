static esp_rmaker_device_t *s_{{prefix_lc}}_rmp_device = NULL;
static esp_rmaker_param_t *s_{{prefix_lc}}_rmp_param = NULL;

/* The bus value as the app shows it. */
static esp_rmaker_param_val_t {{prefix_lc}}_rmp_val(app_driver_param_val_t v)
{
#if defined({{prefix}}_RMP_KIND_time)
    static char {{prefix_lc}}_hhmm[6];
    unsigned m = (unsigned)v.u16 % 1440U;
    snprintf({{prefix_lc}}_hhmm, sizeof {{prefix_lc}}_hhmm, "%02u:%02u", m / 60U, m % 60U);
    return esp_rmaker_str({{prefix_lc}}_hhmm);
#elif defined({{prefix}}_RMP_VT_bool)
    return esp_rmaker_bool(v.b);
#elif defined({{prefix}}_RMP_VT_u8)
    return esp_rmaker_int(v.u8);
#elif defined({{prefix}}_RMP_VT_i16)
    return esp_rmaker_int(v.i16);
#elif defined({{prefix}}_RMP_VT_u16)
    return esp_rmaker_int(v.u16);
#else
    return esp_rmaker_int((int)v.u32);
#endif
}
