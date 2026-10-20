#define {{prefix}}_IMU_PORT      {{cfg.i2c_port}}
#define {{prefix}}_IMU_ADDR      {{cfg.i2c_address}}
#define {{prefix}}_IMU_FREQ_HZ   {{cfg.i2c_freq_hz}}
#define {{prefix}}_IMU_POLL_MS   {{cfg.poll_interval_ms}}
#define {{prefix}}_IMU_THRESH_MG {{cfg.motion_threshold_mg}}
#define {{prefix}}_IMU_HOLD_MS   {{cfg.motion_hold_ms}}
/* MPU6050-family registers (accelerometer path). */
#define {{prefix}}_IMU_REG_SMPLRT_DIV   0x19
#define {{prefix}}_IMU_REG_CONFIG       0x1A
#define {{prefix}}_IMU_REG_ACCEL_CONFIG 0x1C
#define {{prefix}}_IMU_REG_ACCEL_XOUT_H 0x3B
#define {{prefix}}_IMU_REG_PWR_MGMT_1   0x6B
#define {{prefix}}_IMU_REG_WHO_AM_I     0x75
/* +/-2 g range: 16384 LSB per g. */
#define {{prefix}}_IMU_LSB_PER_G 16384
/* Consecutive failed reads before the poll re-probes and re-configures. */
#define {{prefix}}_IMU_RECOVER_AFTER 10
