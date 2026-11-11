{{#if cfg.mode_param}}
case {{cfg.mode_param}}: {
    if (val.u8 < 4)
        zc_mqtt_publish(s_{{prefix_lc}}_state_topic,
                        s_{{prefix_lc}}_options[val.u8], 0, 1, true);
    return;
}
{{/if}}
