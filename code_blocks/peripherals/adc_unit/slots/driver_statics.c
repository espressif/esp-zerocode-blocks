static adc_oneshot_unit_handle_t s_{{prefix_lc}}_adc = NULL;
adc_oneshot_unit_handle_t {{prefix_lc}}_adc_handle(void) { return s_{{prefix_lc}}_adc; }
