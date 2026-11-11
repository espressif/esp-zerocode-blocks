#if {{prefix}}_WC_SPLIT
if (param_id == {{#if cfg.current_param}}{{cfg.current_param}}{{/if}} && val.u16 <= 10000 && s_{{prefix_lc}}_position_param) {
#else
if (param_id == {{cfg.position_param}} && s_{{prefix_lc}}_position_param) {
#endif
    /* Driver position is Matter centi-percent (0-10000); RainMaker Position is 0-100. */
    int pct = val.u16 / 100;
    if (pct > 100) pct = 100;
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_position_param, esp_rmaker_int(pct));
}
