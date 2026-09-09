/* Primary movement control arrives as cluster commands (not routed to
 * blocks yet — see block.yml note); forward direct attribute writes of
 * the lift percentage defensively. */
if (endpoint == s_{{prefix_lc}}_zb_endpoint &&
    cluster_id == EZB_ZCL_CLUSTER_ID_WINDOW_COVERING &&
    attr_id == EZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_LIFT_PERCENTAGE_ID &&
    message->in.attribute.data.type == EZB_ZCL_ATTR_TYPE_UINT8 &&
    message->in.attribute.data.value) {
    uint8_t pct = *(uint8_t *)message->in.attribute.data.value;
    if (pct > 100) pct = 100;
    /* ZCL lift percentage is 0-100; driver position is centi-percent (0-10000). */
    app_driver_param_val_t pv = { .u16 = (uint16_t)(pct * 100) };
    app_driver_set_param({{cfg.position_param}}, pv, s_handle);
}
