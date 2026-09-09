static uint16_t s_{{prefix_lc}}_endpoint_id = 0;
/* PlayChimeSound -> speaker tone param, auto-silenced after ~2s (shared
 * delegate, see zc_matter_delegates.h). */
static zc::ChimeDelegate s_{{prefix_lc}}_chime_delegate({{cfg.tone_param}});
