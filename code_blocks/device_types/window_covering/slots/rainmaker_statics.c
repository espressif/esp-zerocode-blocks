static esp_rmaker_device_t *s_{{prefix_lc}}_device = NULL;
static esp_rmaker_param_t *s_{{prefix_lc}}_position_param = NULL;
/* 1 when the instance reports a measured position separately (current_param). */
#define {{prefix}}_WC_SPLIT (0{{#if cfg.current_param}} + 1{{/if}})
/* A command: with a measured position (current_param) "go to X" is delivered
 * every time — a repeat after a fault or a manual move still drives the motor. */
#if {{prefix}}_WC_SPLIT
#define {{prefix}}_WC_COMMAND(id, v, h) app_driver_fire_event((id), (v), (h))
#else
#define {{prefix}}_WC_COMMAND(id, v, h) app_driver_set_param((id), (v), (h))
#endif
