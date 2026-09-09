#define {{prefix}}_GAUGE_PORT    {{cfg.i2c_port}}
#define {{prefix}}_GAUGE_ADDR    {{cfg.i2c_address}}
#define {{prefix}}_GAUGE_POLL_MS {{cfg.poll_interval_ms}}
#define {{prefix}}_GAUGE_LOW_PCT {{cfg.low_battery_pct}}
/* BQ27220 standard commands. Each is a 16-bit little-endian read. */
#define {{prefix}}_GAUGE_REG_VOLTAGE 0x08
#define {{prefix}}_GAUGE_REG_SOC     0x2C
#define {{prefix}}_GAUGE_FREQ_HZ  {{cfg.i2c_freq_hz}}
