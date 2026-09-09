case {{cfg.position_param}}: {
    /* Two-tone doorbell chime on any position change (press). */
    static const zc_tone_t {{prefix_lc}}_chime[] = { { 880, 150 }, { 0, 40 }, { 660, 250 } };
    app_sound_play({{prefix_lc}}_chime, 3);
    return;
}
