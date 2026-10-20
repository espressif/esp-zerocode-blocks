/* THE BUS IS SOMEBODY ELSE'S. Whoever owns this port created it — an earlier
 * peripherals/i2c_bus instance in a product that wires its own, or the board
 * manager on a board that owns the port — and ESP-IDF's own registry is the
 * join. So this block names a PORT, asks the driver for that port's bus, and
 * attaches ONE device to it. See CLAUDE.md → How an I2C block gets its bus. */
static i2c_master_dev_handle_t s_{{prefix_lc}}_i2c_dev = NULL;

static esp_err_t {{prefix_lc}}_i2c_attach(void)
{
    if (s_{{prefix_lc}}_i2c_dev != NULL) return ESP_OK;
    i2c_master_bus_handle_t {{prefix_lc}}_bus = NULL;
    esp_err_t {{prefix_lc}}_err = i2c_master_get_bus_handle((i2c_port_num_t){{prefix}}_IMU_PORT, &{{prefix_lc}}_bus);
    if ({{prefix_lc}}_err != ESP_OK) {
        /* Fail CLOSED, and say which port. Aborting would take a whole product
         * down for one absent sensor; staying quiet is how a device ends up
         * silent with nothing in any report saying why. */
        ESP_LOGE(TAG, "{{prefix_lc}}: no I2C bus on port %d (%s) — needs a bus provider EARLIER in the product, or a board that owns the port",
                 (int){{prefix}}_IMU_PORT, esp_err_to_name({{prefix_lc}}_err));
        return {{prefix_lc}}_err;
    }
    /* Field-by-field, not a designated-initializer list: these files compile as
     * C++, where initializer order is fixed and a nested flags struct makes the
     * positional form brittle. SCL speed is per DEVICE under this driver. */
    i2c_device_config_t {{prefix_lc}}_dev_cfg = {};
    {{prefix_lc}}_dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    {{prefix_lc}}_dev_cfg.device_address = {{prefix}}_IMU_ADDR;
    {{prefix_lc}}_dev_cfg.scl_speed_hz = {{prefix}}_IMU_FREQ_HZ;
    {{prefix_lc}}_err = i2c_master_bus_add_device({{prefix_lc}}_bus, &{{prefix_lc}}_dev_cfg, &s_{{prefix_lc}}_i2c_dev);
    if ({{prefix_lc}}_err != ESP_OK) {
        ESP_LOGE(TAG, "{{prefix_lc}}: i2c_master_bus_add_device(0x%02x): %s",
                 (unsigned){{prefix}}_IMU_ADDR, esp_err_to_name({{prefix_lc}}_err));
        s_{{prefix_lc}}_i2c_dev = NULL;
    }
    return {{prefix_lc}}_err;
}

/* {{prefix_lc}}: MPU6050-family accelerometer — motion, vibration level, tilt. */

static bool s_{{prefix_lc}}_present = false;
static bool s_{{prefix_lc}}_motion = false;
static int64_t s_{{prefix_lc}}_last_move_ms = 0;
static int s_{{prefix_lc}}_fail = 0;
static uint32_t s_{{prefix_lc}}_vib_x16 = 0;      /* EMA of |a|-1g, mg x 16 */
static uint16_t s_{{prefix_lc}}_vib_pub = 0xFFFF;  /* last published level   */

static esp_err_t {{prefix_lc}}_rd(uint8_t reg, uint8_t *buf, size_t len)
{
    return i2c_master_transmit_receive(s_{{prefix_lc}}_i2c_dev, &reg, 1, buf, len, 50);
}

static esp_err_t {{prefix_lc}}_wr(uint8_t reg, uint8_t val)
{
    uint8_t b[2] = { reg, val };
    return i2c_master_transmit(s_{{prefix_lc}}_i2c_dev, b, sizeof(b), 50);
}

/* WHO_AM_I of every family member this block drives (see block.yml). */
static bool {{prefix_lc}}_known_id(uint8_t id)
{
    return id == 0x68 || id == 0x70 || id == 0x19 || id == 0x71 || id == 0x73;
}

/* Probe and configure. Register writes only — no delay — so the poll can call
 * it to recover without blocking the shared esp_timer task. Wake comes first:
 * after a power-on or brownout the chip is asleep (PWR_MGMT_1 = 0x40). */
static bool {{prefix_lc}}_configure(void)
{
    uint8_t id = 0;
    if ({{prefix_lc}}_rd({{prefix}}_IMU_REG_WHO_AM_I, &id, 1) != ESP_OK || !{{prefix_lc}}_known_id(id)) {
        return false;
    }
    bool ok = {{prefix_lc}}_wr({{prefix}}_IMU_REG_PWR_MGMT_1, 0x01) == ESP_OK      /* awake, PLL clock */
           && {{prefix_lc}}_wr({{prefix}}_IMU_REG_CONFIG, 0x03) == ESP_OK          /* DLPF ~44 Hz */
           && {{prefix_lc}}_wr({{prefix}}_IMU_REG_SMPLRT_DIV, 0x09) == ESP_OK      /* 1 kHz / 10 = 100 Hz */
           && {{prefix_lc}}_wr({{prefix}}_IMU_REG_ACCEL_CONFIG, 0x00) == ESP_OK;   /* +/-2 g */
    if (ok) ESP_LOGI(TAG, "{{prefix_lc}}: accelerometer WHO_AM_I 0x%02x configured", id);
    return ok;
}

/* Orientation from the gravity vector (block.yml enum). Coarse on purpose. */
static uint8_t {{prefix_lc}}_orientation(int32_t x_mg, int32_t y_mg, int32_t z_mg)
{
    int32_t ax = labs(x_mg), ay = labs(y_mg), az = labs(z_mg);
    if (az > ay && az > ax && az > 700) return z_mg > 0 ? 1 : 2;
    if (ay > ax && ay > 700)            return y_mg > 0 ? 3 : 4;
    if (ax > 700)                       return x_mg > 0 ? 5 : 6;
    return 0;
}

/* Integer square root (no libm on the timer task). */
static uint32_t {{prefix_lc}}_isqrt(uint64_t v)
{
    uint64_t r = 0, bit = 1ULL << 62;
    while (bit > v) bit >>= 2;
    while (bit) {
        if (v >= r + bit) { v -= r + bit; r = (r >> 1) + bit; } else { r >>= 1; }
        bit >>= 2;
    }
    return (uint32_t)r;
}

static void {{prefix_lc}}_poll_cb(void *arg)
{
    (void)arg;
    if (!s_{{prefix_lc}}_present) return;

    uint8_t d[6] = {0};
    if ({{prefix_lc}}_rd({{prefix}}_IMU_REG_ACCEL_XOUT_H, d, sizeof(d)) != ESP_OK) {
        if (++s_{{prefix_lc}}_fail >= {{prefix}}_IMU_RECOVER_AFTER) {
            s_{{prefix_lc}}_fail = 0;
            if ({{prefix_lc}}_configure()) ESP_LOGW(TAG, "{{prefix_lc}}: accelerometer recovered");
            else ESP_LOGW(TAG, "{{prefix_lc}}: accelerometer not answering at 0x%02x", {{prefix}}_IMU_ADDR);
        }
        return;
    }
    s_{{prefix_lc}}_fail = 0;
    /* Big-endian X, Y, Z. */
    int16_t x = (int16_t)((uint16_t)d[0] << 8 | d[1]);
    int16_t y = (int16_t)((uint16_t)d[2] << 8 | d[3]);
    int16_t z = (int16_t)((uint16_t)d[4] << 8 | d[5]);
    /* A sleeping chip (brownout) reads all zeros: wake it rather than report 0 g. */
    if (x == 0 && y == 0 && z == 0) { {{prefix_lc}}_configure(); return; }

    int32_t x_mg = ((int32_t)x * 1000) / {{prefix}}_IMU_LSB_PER_G;
    int32_t y_mg = ((int32_t)y * 1000) / {{prefix}}_IMU_LSB_PER_G;
    int32_t z_mg = ((int32_t)z * 1000) / {{prefix}}_IMU_LSB_PER_G;

    /* Motion from the MAGNITUDE's deviation from 1 g: orientation-independent,
     * no reference sample needed. */
    uint32_t mag = {{prefix_lc}}_isqrt((uint64_t)((int64_t)x_mg * x_mg + (int64_t)y_mg * y_mg + (int64_t)z_mg * z_mg));
    uint32_t dev = mag > 1000 ? mag - 1000 : 1000 - mag;

    int64_t now_ms = esp_timer_get_time() / 1000;
    if (dev > {{prefix}}_IMU_THRESH_MG) s_{{prefix_lc}}_last_move_ms = now_ms;
    bool moving = s_{{prefix_lc}}_last_move_ms != 0 &&
                  (now_ms - s_{{prefix_lc}}_last_move_ms) < {{prefix}}_IMU_HOLD_MS;
    if (moving != s_{{prefix_lc}}_motion) {
        s_{{prefix_lc}}_motion = moving;
        app_driver_param_val_t v = { .b = moving };
        app_driver_set_param({{cfg.motion_param}}, v, APP_DRIVER_SOURCE_LOCAL);
        ESP_LOGI(TAG, "{{prefix_lc}}: motion %s", moving ? "started" : "stopped");
    }
{{#if cfg.vibration_param}}
    {
        /* EMA with alpha 1/16, kept x16 for precision: settles in ~1-2 s at 50 Hz. */
        s_{{prefix_lc}}_vib_x16 += dev - (s_{{prefix_lc}}_vib_x16 >> 4);
        uint32_t lvl = s_{{prefix_lc}}_vib_x16 >> 4;
        uint16_t level = lvl > 0xFFFF ? 0xFFFF : (uint16_t)lvl;
        if (s_{{prefix_lc}}_vib_pub == 0xFFFF || abs((int)level - (int)s_{{prefix_lc}}_vib_pub) >= 5) {
            s_{{prefix_lc}}_vib_pub = level;
            app_driver_param_val_t vv = { .u16 = level };
            app_driver_set_param({{cfg.vibration_param}}, vv, APP_DRIVER_SOURCE_LOCAL);
        }
    }
{{/if}}
{{#if cfg.orientation_param}}
    {
        static uint8_t last_or = 255;
        uint8_t o = {{prefix_lc}}_orientation(x_mg, y_mg, z_mg);
        if (o != last_or) {
            last_or = o;
            app_driver_param_val_t ov = { .u8 = o };
            app_driver_set_param({{cfg.orientation_param}}, ov, APP_DRIVER_SOURCE_LOCAL);
        }
    }
{{/if}}
}
