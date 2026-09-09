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
    esp_err_t {{prefix_lc}}_err = i2c_master_get_bus_handle((i2c_port_num_t){{prefix}}_INA_PORT, &{{prefix_lc}}_bus);
    if ({{prefix_lc}}_err != ESP_OK) {
        /* Fail CLOSED, and say which port. Aborting would take a whole product
         * down for one absent sensor; staying quiet is how a device ends up
         * silent with nothing in any report saying why. */
        ESP_LOGE(TAG, "{{prefix_lc}}: no I2C bus on port %d (%s) — needs a bus provider EARLIER in the product, or a board that owns the port",
                 (int){{prefix}}_INA_PORT, esp_err_to_name({{prefix_lc}}_err));
        return {{prefix_lc}}_err;
    }
    /* Field-by-field, not a designated-initializer list: these files compile as
     * C++, where initializer order is fixed and a nested flags struct makes the
     * positional form brittle. SCL speed is per DEVICE under this driver. */
    i2c_device_config_t {{prefix_lc}}_dev_cfg = {};
    {{prefix_lc}}_dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    {{prefix_lc}}_dev_cfg.device_address = {{prefix}}_INA_ADDR;
    {{prefix_lc}}_dev_cfg.scl_speed_hz = {{prefix}}_INA_FREQ_HZ;
    {{prefix_lc}}_err = i2c_master_bus_add_device({{prefix_lc}}_bus, &{{prefix_lc}}_dev_cfg, &s_{{prefix_lc}}_i2c_dev);
    if ({{prefix_lc}}_err != ESP_OK) {
        ESP_LOGE(TAG, "{{prefix_lc}}: i2c_master_bus_add_device(0x%02x): %s",
                 (unsigned){{prefix}}_INA_ADDR, esp_err_to_name({{prefix_lc}}_err));
        s_{{prefix_lc}}_i2c_dev = NULL;
    }
    return {{prefix_lc}}_err;
}

static esp_err_t {{prefix_lc}}_ina_read_reg(uint8_t reg, uint16_t *out)
{
    esp_err_t err = i2c_master_transmit(s_{{prefix_lc}}_i2c_dev, &reg, 1, 50);
    if (err != ESP_OK) return err;
    uint8_t buf[2] = {0};
    err = i2c_master_receive(s_{{prefix_lc}}_i2c_dev, buf, sizeof(buf), 50);
    if (err != ESP_OK) return err;
    *out = ((uint16_t)buf[0] << 8) | buf[1];
    return ESP_OK;
}

static void {{prefix_lc}}_ina_poll_cb(void *arg)
{
    uint16_t bus = 0, shunt = 0;
    if ({{prefix_lc}}_ina_read_reg(0x02, &bus) != ESP_OK) return;
    if ({{prefix_lc}}_ina_read_reg(0x01, &shunt) != ESP_OK) return;
    /* Bus voltage register: bits 15-3 = mV / 4 (4 mV LSB after shift) */
    uint32_t bus_mv = ((uint32_t)bus >> 3) * 4U;
    /* Shunt voltage register: int16 in 10 µV LSB.
     * Current (mA) = shunt_uV / shunt_mΩ = (raw * 10) / shunt_mΩ */
    int16_t shunt_signed = (int16_t)shunt;
    int32_t current_ma = ((int32_t)shunt_signed * 10) / (int32_t){{prefix}}_INA_SHUNT_MOHM;
    if (current_ma > 32767) current_ma = 32767;
    if (current_ma < -32768) current_ma = -32768;
    app_driver_param_val_t bv = { .u32 = bus_mv };
    app_driver_set_param({{cfg.bus_voltage_mv_param}}, bv, APP_DRIVER_SOURCE_LOCAL);
    app_driver_param_val_t cv = { .i16 = (int16_t)current_ma };
    app_driver_set_param({{cfg.current_ma_param}}, cv, APP_DRIVER_SOURCE_LOCAL);
}
