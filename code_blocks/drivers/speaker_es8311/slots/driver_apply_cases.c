case {{cfg.tone_param}}: {
    s_{{prefix_lc}}_tone_hz = val.u32;
    return ESP_OK;
}
{{#if cfg.volume_param}}
case {{cfg.volume_param}}: {
    s_{{prefix_lc}}_volume = (val.u8 > 100) ? 100 : val.u8;
    return ESP_OK;
}
{{/if}}
{{#if cfg.mute_param}}
case {{cfg.mute_param}}: {
    s_{{prefix_lc}}_muted = val.b;
    return ESP_OK;
}
{{/if}}
