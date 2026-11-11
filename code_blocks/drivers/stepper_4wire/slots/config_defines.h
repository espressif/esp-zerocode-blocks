#define {{prefix}}_ST_IN1     {{cfg.in1_gpio}}
#define {{prefix}}_ST_IN2     {{cfg.in2_gpio}}
#define {{prefix}}_ST_IN3     {{cfg.in3_gpio}}
#define {{prefix}}_ST_IN4     {{cfg.in4_gpio}}
#define {{prefix}}_ST_DELAY_US {{cfg.step_delay_us}}
/* esp_timer refuses a periodic timer under 50 us: a smaller step delay is
   raised to it (block.yml), never left to fail and stall the motor. */
#define {{prefix}}_ST_PERIOD_US (({{prefix}}_ST_DELAY_US) < 50 ? 50 : ({{prefix}}_ST_DELAY_US))
