static char s_{{prefix_lc}}_pos_topic[72] = {0};
static char s_{{prefix_lc}}_setpos_topic[72] = {0};
static char s_{{prefix_lc}}_cmd_topic[72] = {0};
/* 1 when the instance reports a measured position separately (current_param). */
#define {{prefix}}_WC_SPLIT (0{{#if cfg.current_param}} + 1{{/if}})
