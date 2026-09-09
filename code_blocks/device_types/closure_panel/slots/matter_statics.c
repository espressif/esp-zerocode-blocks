static uint16_t s_{{prefix_lc}}_endpoint_id = 0;
/* Controller SetTarget -> position param (shared delegate, see
 * zc_matter_delegates.h). */
static zc::ClosureDimensionDelegate s_{{prefix_lc}}_panel_delegate({{cfg.position_param}});
