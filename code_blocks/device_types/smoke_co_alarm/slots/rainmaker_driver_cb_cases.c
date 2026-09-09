/* State encoding is the driver's own: 0=Normal, 1=Warning, 2=Critical. */
if (param_id == {{cfg.smoke_state_param}} && s_{{prefix_lc}}_smoke_param) {
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_smoke_param, esp_rmaker_int(val.u8));
}
if (param_id == {{cfg.co_state_param}} && s_{{prefix_lc}}_co_param) {
    esp_rmaker_param_update_and_report(s_{{prefix_lc}}_co_param, esp_rmaker_int(val.u8));
}
