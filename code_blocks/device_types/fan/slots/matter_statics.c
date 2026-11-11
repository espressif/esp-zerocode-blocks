static uint16_t s_{{prefix_lc}}_endpoint_id = 0;
/* Controller writes (FanMode, PercentSetting) reach the driver through this
 * delegate, and it reports PercentCurrent — see zc_matter_fan.h. */
static zc::FanDrive s_{{prefix_lc}}_fan({{cfg.speed_param}});
