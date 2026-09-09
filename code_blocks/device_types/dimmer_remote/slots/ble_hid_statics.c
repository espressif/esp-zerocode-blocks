/* {{prefix_lc}}: volume-knob mapping — level_param deltas become volume
 * keys; the u8 wraps, so the diff is taken as a signed byte. */
static uint8_t s_{{prefix_lc}}_prev_level = 0;
static bool s_{{prefix_lc}}_level_seen = false;
