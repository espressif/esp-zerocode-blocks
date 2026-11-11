#define {{prefix}}_DSC_WAKE_GPIO      {{cfg.wake_gpio}}
#define {{prefix}}_DSC_WAKE_GPIO_2    {{cfg.wake_gpio_2}}
#define {{prefix}}_DSC_HEARTBEAT_MIN  {{cfg.heartbeat_minutes}}
#define {{prefix}}_DSC_MAX_AWAKE_MS   {{cfg.max_awake_ms}}
#define {{prefix}}_DSC_QUIET_MS       {{cfg.quiet_ms}}
#define {{prefix}}_DSC_BOOT_AWAKE_S   {{cfg.boot_awake_seconds}}
#define {{prefix}}_DSC_USB_AWAKE      {{cfg.stay_awake_on_usb}}
/* The pull as a token the preprocessor can test (string params are not). */
#define {{prefix}}_DSC_PULL_{{cfg.wake_pull}} 1
/* What the network report hook answers (mqtt_statics -> logic). */
#define {{prefix}}_DSC_NET_UNCONFIGURED 0
#define {{prefix}}_DSC_NET_PENDING      1
#define {{prefix}}_DSC_NET_REPORTED     2
