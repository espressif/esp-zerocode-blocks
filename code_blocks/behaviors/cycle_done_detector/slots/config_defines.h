#define {{prefix}}_CYC_MIN_LEVEL   {{cfg.min_level}}
#define {{prefix}}_CYC_MARGIN      {{cfg.margin}}
#define {{prefix}}_CYC_IDLE_LEVEL  {{cfg.idle_level}}
#define {{prefix}}_CYC_WINDOW_S    {{cfg.window_seconds}}
#define {{prefix}}_CYC_START_S     {{cfg.start_active_seconds}}
#define {{prefix}}_CYC_BUSY_S      {{cfg.busy_active_seconds}}
#define {{prefix}}_CYC_MIN_RUN_S   ({{cfg.min_run_minutes}} * 60)
#define {{prefix}}_CYC_QUIET_S     ({{cfg.quiet_minutes}} * 60)
#define {{prefix}}_CYC_NVS_NS      "{{cfg.nvs_namespace}}"
#define {{prefix}}_CYC_NVS_KEY     "{{cfg.nvs_key}}"
/* A cycle running when the device restarted: true = carry on, false = drop it.
 * Tested with a plain if, never #if: a C preprocessor reads `true` as 0. */
#define {{prefix}}_CYC_RESUME      ({{cfg.resume_after_reboot}})
