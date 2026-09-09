static uint16_t s_{{prefix_lc}}_endpoint_id = 0;
static zc::OperationalStateDelegate s_{{prefix_lc}}_opstate({{cfg.state_param}});
{{#if cfg.mode_param}}
static const zc::ModeOption s_{{prefix_lc}}_modes[] = { {"Normal", 0, 0x4000}, {"Delicate", 1, 0x4001}, {"Heavy", 2, 0x4002}, {"Whites", 3, 0x4003} };
static zc::ModeDelegate s_{{prefix_lc}}_mode({{cfg.mode_param}}, s_{{prefix_lc}}_modes,
    sizeof(s_{{prefix_lc}}_modes) / sizeof(s_{{prefix_lc}}_modes[0]));
{{/if}}
