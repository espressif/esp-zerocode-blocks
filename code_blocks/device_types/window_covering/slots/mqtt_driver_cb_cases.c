#if {{prefix}}_WC_SPLIT
{{#if cfg.current_param}}case {{cfg.current_param}}: {
    if (val.u16 > 10000) return;   /* unknown: keep the last reported position */
    char buf[8]; snprintf(buf, sizeof(buf), "%u", (unsigned)(val.u16 / 100));
    zc_mqtt_publish(s_{{prefix_lc}}_pos_topic, buf, 0, 1, true);
    return;
}{{/if}}
#else
case {{cfg.position_param}}: {
    char buf[8]; snprintf(buf, sizeof(buf), "%u", (unsigned)(val.u16 / 100));
    zc_mqtt_publish(s_{{prefix_lc}}_pos_topic, buf, 0, 1, true);
    return;
}
#endif
