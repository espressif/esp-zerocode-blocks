static uint8_t s_{{prefix_lc}}_zb_endpoint = 0;
/* 1 when the instance reports a measured position separately (current_param). */
#define {{prefix}}_WC_SPLIT (0{{#if cfg.current_param}} + 1{{/if}})
