#define {{prefix}}_SERVO_GPIO     {{cfg.gpio}}
#define {{prefix}}_SERVO_CHANNEL  {{cfg.ledc_channel}}
#define {{prefix}}_SERVO_TIMER    {{cfg.ledc_timer}}
/* Position the servo is energized at from boot until the first param arrives.
 * Not the middle of travel — see block.yml for why. */
#define {{prefix}}_SERVO_REST_DEG {{cfg.rest_deg}}
/* 50 Hz period = 20 ms = 16384 ticks (14-bit); 1.0 ms = 819, 2.0 ms = 1638. */
#define {{prefix}}_SERVO_DUTY_FOR_DEG(deg) (819U + ((uint32_t)(deg) * 819U) / 180U)
