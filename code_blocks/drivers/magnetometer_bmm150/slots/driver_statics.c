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
    esp_err_t {{prefix_lc}}_err = i2c_master_get_bus_handle((i2c_port_num_t){{prefix}}_MAG_PORT, &{{prefix_lc}}_bus);
    if ({{prefix_lc}}_err != ESP_OK) {
        /* Fail CLOSED, and say which port. Aborting would take a whole product
         * down for one absent sensor; staying quiet is how a device ends up
         * silent with nothing in any report saying why. */
        ESP_LOGE(TAG, "{{prefix_lc}}: no I2C bus on port %d (%s) — needs a bus provider EARLIER in the product, or a board that owns the port",
                 (int){{prefix}}_MAG_PORT, esp_err_to_name({{prefix_lc}}_err));
        return {{prefix_lc}}_err;
    }
    /* Field-by-field, not a designated-initializer list: these files compile as
     * C++, where initializer order is fixed and a nested flags struct makes the
     * positional form brittle. SCL speed is per DEVICE under this driver. */
    i2c_device_config_t {{prefix_lc}}_dev_cfg = {};
    {{prefix_lc}}_dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    {{prefix_lc}}_dev_cfg.device_address = {{prefix}}_MAG_ADDR;
    {{prefix_lc}}_dev_cfg.scl_speed_hz = {{prefix}}_MAG_FREQ_HZ;
    {{prefix_lc}}_err = i2c_master_bus_add_device({{prefix_lc}}_bus, &{{prefix_lc}}_dev_cfg, &s_{{prefix_lc}}_i2c_dev);
    if ({{prefix_lc}}_err != ESP_OK) {
        ESP_LOGE(TAG, "{{prefix_lc}}: i2c_master_bus_add_device(0x%02x): %s",
                 (unsigned){{prefix}}_MAG_ADDR, esp_err_to_name({{prefix_lc}}_err));
        s_{{prefix_lc}}_i2c_dev = NULL;
    }
    return {{prefix_lc}}_err;
}

/* {{prefix_lc}}: BMM150 magnetometer — field magnitude + magnet-present. */

static bool s_{{prefix_lc}}_present_dev = false;

static esp_err_t {{prefix_lc}}_rd(uint8_t reg, uint8_t *buf, size_t len)
{
    return i2c_master_transmit_receive(s_{{prefix_lc}}_i2c_dev, &reg, 1, buf, len, 50);
}

static esp_err_t {{prefix_lc}}_wr(uint8_t reg, uint8_t val)
{
    uint8_t b[2] = { reg, val };
    return i2c_master_transmit(s_{{prefix_lc}}_i2c_dev, b, sizeof(b), 50);
}

/* Integer hypotenuse of three axes, so no float maths runs every 200 ms.
 * 64-bit throughout: the inputs below are in 1/32 uT and reach ~82k, whose
 * square overflows int32 and whose sum passes 2^32. */
static uint32_t {{prefix_lc}}_mag3(int32_t x, int32_t y, int32_t z)
{
    uint64_t sq = (uint64_t)((int64_t)x * x) + (uint64_t)((int64_t)y * y) + (uint64_t)((int64_t)z * z);
    uint64_t r = 0, bit = 1ULL << 62;
    while (bit > sq) bit >>= 2;
    while (bit) {
        if (sq >= r + bit) { sq -= r + bit; r = (r >> 1) + bit; }
        else r >>= 1;
        bit >>= 2;
    }
    return (uint32_t)r;
}

static void {{prefix_lc}}_poll_cb(void *arg)
{
    (void)arg;
    if (!s_{{prefix_lc}}_present_dev) return;

    uint8_t d[8] = {0};
    if ({{prefix_lc}}_rd({{prefix}}_MAG_REG_DATA, d, sizeof(d)) != ESP_OK) return;

    /* X and Y are 13-bit, Z is 15-bit, each left-aligned in its pair with the
     * low bits used for other flags — shifting back down is what recovers the
     * sign. */
    int16_t x = (int16_t)(((int16_t)((uint16_t)d[1] << 8 | (d[0] & 0xF8))) >> 3);
    int16_t y = (int16_t)(((int16_t)((uint16_t)d[3] << 8 | (d[2] & 0xF8))) >> 3);
    int16_t z = (int16_t)(((int16_t)((uint16_t)d[5] << 8 | (d[4] & 0xFE))) >> 1);

    /* Raw counts are NOT microtesla. X/Y span +/-1300 uT over 13 bits
     * (~0.3125 uT/LSB) and Z +/-2500 uT over 15 bits (~0.156 uT/LSB), so raw
     * counts read 3-6x high and weight Z differently — Earth's field alone
     * could reach a 300-count threshold. Scale each axis to 1/32 uT (X/Y x10,
     * Z x5), take the magnitude, divide by 32. Nominal scale only: the
     * datasheet's full compensation needs per-part trim registers this block
     * does not read, so expect ~10% and a small offset — fine for "is a
     * magnet near", not for a compass. */
    uint32_t ut = ({{prefix_lc}}_mag3((int32_t)x * 10, (int32_t)y * 10, (int32_t)z * 5) + 16) / 32;
    if (ut > 65535) ut = 65535;

    app_driver_param_val_t v = { .u16 = (uint16_t)ut };
    app_driver_set_param({{cfg.field_param}}, v, APP_DRIVER_SOURCE_LOCAL);
{{#if cfg.magnet_param}}
    {
        /* Edge-triggered: an accessory sitting on the board must not republish
         * "magnet present" five times a second. */
        static int last = -1;
        int near = (ut > {{prefix}}_MAG_THRESH_UT) ? 1 : 0;
        if (near != last) {
            last = near;
            app_driver_param_val_t nv = { .b = near != 0 };
            app_driver_set_param({{cfg.magnet_param}}, nv, APP_DRIVER_SOURCE_LOCAL);
            ESP_LOGI(TAG, "{{prefix_lc}}: magnet %s (%lu uT)",
                     near ? "detected" : "removed", (unsigned long)ut);
        }
    }
{{/if}}
}
