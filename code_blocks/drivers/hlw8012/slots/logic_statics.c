extern pcnt_unit_handle_t s_{{prefix_lc}}_hlw_pcnt;

static void {{prefix_lc}}_hlw_poll_cb(void *arg)
{
    int count = 0;
    pcnt_unit_get_count(s_{{prefix_lc}}_hlw_pcnt, &count);
    pcnt_unit_clear_count(s_{{prefix_lc}}_hlw_pcnt);
    if (count < 0) count = 0;
    /* mW = scale (mW/Hz) * Hz, Hz = count * 1000 / window_ms. All in 64-bit:
       count * 1e6 overflowed 32 bits above 4294 pulses per window. */
    uint64_t mw64 = ((uint64_t){{prefix}}_HLW_SCALE * (uint64_t)count * 1000ULL) / (uint64_t){{prefix}}_HLW_WINDOW_MS;
    uint32_t mw = mw64 > 0xFFFFFFFFULL ? 0xFFFFFFFFU : (uint32_t)mw64;
    app_driver_param_val_t v = { .u32 = mw };
    app_driver_set_param({{cfg.power_param}}, v, APP_DRIVER_SOURCE_LOCAL);
}
