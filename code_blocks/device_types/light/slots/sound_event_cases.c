case {{cfg.power_param}}: {
    /* Soft UI click on toggle — audible confirmation, deliberately short. */
    static const zc_tone_t {{prefix_lc}}_click[] = { { 1200, 35 } };
    app_sound_play({{prefix_lc}}_click, 1);
    return;
}
