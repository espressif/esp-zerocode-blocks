#define {{prefix}}_IMU_PORT      {{cfg.i2c_port}}
#define {{prefix}}_IMU_ADDR      {{cfg.i2c_address}}
#define {{prefix}}_IMU_POLL_MS   {{cfg.poll_interval_ms}}
#define {{prefix}}_IMU_THRESH_MG {{cfg.motion_threshold_mg}}
#define {{prefix}}_IMU_HOLD_MS   {{cfg.motion_hold_ms}}
/* BMI270 registers used here. Accelerometer path only — see block.yml on why
   the firmware blob is not loaded. */
#define {{prefix}}_IMU_REG_CHIP_ID 0x00
#define {{prefix}}_IMU_REG_PWR_CTRL 0x7D
#define {{prefix}}_IMU_REG_ACC_CONF 0x40
#define {{prefix}}_IMU_REG_ACC_RANGE 0x41
#define {{prefix}}_IMU_REG_DATA_ACC 0x0C
#define {{prefix}}_IMU_CHIP_ID_VAL 0x24
/* +/-2 g range: 16384 LSB per g. Tilt and a shake do not need more. */
#define {{prefix}}_IMU_LSB_PER_G 16384
#define {{prefix}}_IMU_FREQ_HZ  {{cfg.i2c_freq_hz}}
