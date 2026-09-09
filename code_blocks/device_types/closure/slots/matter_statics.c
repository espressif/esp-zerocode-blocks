static uint16_t s_{{prefix_lc}}_endpoint_id = 0;
/* Controller MoveTo/Stop -> motor speed param (shared delegate, see
 * zc_matter_delegates.h). */
static zc::ClosureControlDelegate s_{{prefix_lc}}_closure_delegate({{cfg.motor_param}});
