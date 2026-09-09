extern pcnt_unit_handle_t s_{{prefix_lc}}_hlw_pcnt;

static void {{prefix_lc}}_hlw_poll_cb(void *arg)
{
    int count = 0;
    pcnt_unit_get_count(s_{{prefix_lc}}_hlw_pcnt, &count);
    pcnt_unit_clear_count(s_{{prefix_lc}}_hlw_pcnt);
    if (count < 0) count = 0;
    /* Hz = count / window_seconds = count * 1000 / window_ms */
    uint32_t hz_x1000 = ((uint32_t)count * 1000U * 1000U) / (uint32_t){{prefix}}_HLW_WINDOW_MS;
    /* mW = scale * Hz = scale * hz_x1000 / 1000 */
    uint32_t mw = ((uint64_t){{prefix}}_HLW_SCALE * hz_x1000) / 1000000U;
    app_driver_param_val_t v = { .u32 = mw };
    app_driver_set_param({{cfg.power_param}}, v, APP_DRIVER_SOURCE_LOCAL);
}
