if (param_id == {{cfg.speed_param}} && s_{{prefix_lc}}_speed_param) {
    int pct = val.u8 > 100 ? 100 : val.u8;
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_speed_param, esp_rmaker_int(pct));
}
