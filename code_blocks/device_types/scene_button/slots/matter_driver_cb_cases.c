/* One notification = one press, as in every other binding of this block
 * (drivers/momentary_button FIRES it on each click). A position of 0 is a
 * release report and carries no press. */
if (param_id == {{cfg.position_param}} && val.u8 != 0) {
    zc_switch_press(s_{{prefix_lc}}_endpoint_id, val.u8);
}
