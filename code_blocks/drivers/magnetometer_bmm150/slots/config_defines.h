#define {{prefix}}_MAG_PORT      {{cfg.i2c_port}}
#define {{prefix}}_MAG_ADDR      {{cfg.i2c_address}}
#define {{prefix}}_MAG_POLL_MS   {{cfg.poll_interval_ms}}
#define {{prefix}}_MAG_THRESH_UT {{cfg.present_threshold_ut}}
/* BMM150 registers. */
#define {{prefix}}_MAG_REG_CHIP_ID  0x40
#define {{prefix}}_MAG_REG_POWER    0x4B  /* bit0 = power control (suspend/sleep) */
#define {{prefix}}_MAG_REG_OPMODE   0x4C  /* opmode + ODR */
#define {{prefix}}_MAG_REG_DATA     0x42
#define {{prefix}}_MAG_CHIP_ID_VAL  0x32
#define {{prefix}}_MAG_FREQ_HZ  {{cfg.i2c_freq_hz}}
