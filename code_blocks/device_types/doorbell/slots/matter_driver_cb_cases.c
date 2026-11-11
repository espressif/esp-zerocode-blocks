/* One notification = one ring. drivers/button TOGGLES press_param, so a
 * press shows up as a change in EITHER direction; keying on "true" alone
 * would ring on every other press. */
if (param_id == {{cfg.press_param}}) {
    zc_switch_press(s_{{prefix_lc}}_endpoint_id, 1);
}
