case {{cfg.air_quality_param}}: {
    if (s_{{prefix_lc}}_aqi_label != NULL)
        lv_label_set_text_fmt(s_{{prefix_lc}}_aqi_label, "AQI %u", (unsigned)val.u8);
    break;
}
