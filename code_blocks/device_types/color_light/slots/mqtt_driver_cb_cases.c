case {{cfg.power_param}}:
case {{cfg.brightness_param}}:
case {{cfg.hue_param}}:
case {{cfg.saturation_param}}: {
    s_{{prefix_lc}}_publish_state();
    return;
}
