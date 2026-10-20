/* Smoke/CO state goes through the cluster object, not attribute::update() —
 * see zc_smoke_co_push. Both params are pushed together so ExpressedState is
 * recomputed from the pair. */
if (param_id == {{cfg.smoke_state_param}} || param_id == {{cfg.co_state_param}}) {
    app_driver_param_val_t smoke = {}, co = {};
    app_driver_get_param({{cfg.smoke_state_param}}, &smoke);
    app_driver_get_param({{cfg.co_state_param}}, &co);
    if (param_id == {{cfg.smoke_state_param}}) smoke = val;
    else co = val;
    zc_smoke_co_push(s_{{prefix_lc}}_endpoint_id, smoke.u8, co.u8);
}
