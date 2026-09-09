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

/* {{prefix_lc}}: BMI270 accelerometer — tilt + motion, no firmware blob.
 * Register access goes through the device handle attached above — the same
 * shape every other sensor block here uses, so they all share one bus. */

static bool s_{{prefix_lc}}_present = false;
static bool s_{{prefix_lc}}_motion = false;
static int64_t s_{{prefix_lc}}_last_move_ms = 0;

static esp_err_t {{prefix_lc}}_rd(uint8_t reg, uint8_t *buf, size_t len)
{
    return i2c_master_transmit_receive(s_{{prefix_lc}}_i2c_dev, &reg, 1, buf, len, 50);
}

static esp_err_t {{prefix_lc}}_wr(uint8_t reg, uint8_t val)
{
    uint8_t b[2] = { reg, val };
    return i2c_master_transmit(s_{{prefix_lc}}_i2c_dev, b, sizeof(b), 50);
}

/* Orientation from the gravity vector: whichever axis carries most of 1 g, and
 * its sign. Deliberately coarse — the point is "which way is the panel facing",
 * not a quaternion. Returns the block.yml enum. */
static uint8_t {{prefix_lc}}_orientation(int32_t x_mg, int32_t y_mg, int32_t z_mg)
{
    int32_t ax = labs(x_mg), ay = labs(y_mg), az = labs(z_mg);
    /* 700 mg: comfortably past 45 degrees, so a tilted board reads as one
       orientation rather than flickering between two. */
    if (az > ay && az > ax && az > 700) return z_mg > 0 ? 1 : 2;
    if (ay > ax && ay > 700)            return y_mg > 0 ? 3 : 4;
    if (ax > 700)                       return x_mg > 0 ? 5 : 6;
    return 0;
}

static void {{prefix_lc}}_poll_cb(void *arg)
{
    (void)arg;
    if (!s_{{prefix_lc}}_present) return;

    uint8_t d[6] = {0};
    if ({{prefix_lc}}_rd({{prefix}}_IMU_REG_DATA_ACC, d, sizeof(d)) != ESP_OK) return;
    int16_t x = (int16_t)((uint16_t)d[1] << 8 | d[0]);
    int16_t y = (int16_t)((uint16_t)d[3] << 8 | d[2]);
    int16_t z = (int16_t)((uint16_t)d[5] << 8 | d[4]);

    int32_t x_mg = ((int32_t)x * 1000) / {{prefix}}_IMU_LSB_PER_G;
    int32_t y_mg = ((int32_t)y * 1000) / {{prefix}}_IMU_LSB_PER_G;
    int32_t z_mg = ((int32_t)z * 1000) / {{prefix}}_IMU_LSB_PER_G;

    /* Motion from the MAGNITUDE's deviation from 1 g, not from per-axis deltas:
     * a board sitting still reads ~1000 mg however it is oriented, so this needs
     * no reference sample and does not false-trigger when someone simply turns
     * the panel around. sqrt is avoided by comparing squares. */
    int64_t mag_sq = (int64_t)x_mg * x_mg + (int64_t)y_mg * y_mg + (int64_t)z_mg * z_mg;
    int64_t lo = (int64_t)(1000 - {{prefix}}_IMU_THRESH_MG) * (1000 - {{prefix}}_IMU_THRESH_MG);
    int64_t hi = (int64_t)(1000 + {{prefix}}_IMU_THRESH_MG) * (1000 + {{prefix}}_IMU_THRESH_MG);

    int64_t now_ms = esp_timer_get_time() / 1000;
    if (mag_sq < lo || mag_sq > hi) s_{{prefix_lc}}_last_move_ms = now_ms;
    bool moving = s_{{prefix_lc}}_last_move_ms != 0 &&
                  (now_ms - s_{{prefix_lc}}_last_move_ms) < {{prefix}}_IMU_HOLD_MS;

    if (moving != s_{{prefix_lc}}_motion) {
        s_{{prefix_lc}}_motion = moving;
        app_driver_param_val_t v = { .b = moving };
        app_driver_set_param({{cfg.motion_param}}, v, APP_DRIVER_SOURCE_LOCAL);
        ESP_LOGI(TAG, "{{prefix_lc}}: motion %s", moving ? "started" : "stopped");
    }
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
