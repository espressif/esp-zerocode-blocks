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
    esp_err_t {{prefix_lc}}_err = i2c_master_get_bus_handle((i2c_port_num_t){{prefix}}_SCD_PORT, &{{prefix_lc}}_bus);
    if ({{prefix_lc}}_err != ESP_OK) {
        /* Fail CLOSED, and say which port. Aborting would take a whole product
         * down for one absent sensor; staying quiet is how a device ends up
         * silent with nothing in any report saying why. */
        ESP_LOGE(TAG, "{{prefix_lc}}: no I2C bus on port %d (%s) — needs a bus provider EARLIER in the product, or a board that owns the port",
                 (int){{prefix}}_SCD_PORT, esp_err_to_name({{prefix_lc}}_err));
        return {{prefix_lc}}_err;
    }
    /* Field-by-field, not a designated-initializer list: these files compile as
     * C++, where initializer order is fixed and a nested flags struct makes the
     * positional form brittle. SCL speed is per DEVICE under this driver. */
    i2c_device_config_t {{prefix_lc}}_dev_cfg = {};
    {{prefix_lc}}_dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    {{prefix_lc}}_dev_cfg.device_address = {{prefix}}_SCD_ADDR;
    {{prefix_lc}}_dev_cfg.scl_speed_hz = {{prefix}}_SCD_FREQ_HZ;
    {{prefix_lc}}_err = i2c_master_bus_add_device({{prefix_lc}}_bus, &{{prefix_lc}}_dev_cfg, &s_{{prefix_lc}}_i2c_dev);
    if ({{prefix_lc}}_err != ESP_OK) {
        ESP_LOGE(TAG, "{{prefix_lc}}: i2c_master_bus_add_device(0x%02x): %s",
                 (unsigned){{prefix}}_SCD_ADDR, esp_err_to_name({{prefix_lc}}_err));
        s_{{prefix_lc}}_i2c_dev = NULL;
    }
    return {{prefix_lc}}_err;
}

static esp_err_t {{prefix_lc}}_scd_send_cmd(uint16_t cmd)
{
    uint8_t buf[2] = { (uint8_t)(cmd >> 8), (uint8_t)(cmd & 0xFF) };
    return i2c_master_transmit(s_{{prefix_lc}}_i2c_dev, buf, sizeof(buf), 50);
}

static uint8_t {{prefix_lc}}_aqi_from_co2(uint16_t ppm)
{
    if (ppm < 600)  return 1; /* Good */
    if (ppm < 1000) return 2; /* Fair */
    if (ppm < 1500) return 3; /* Moderate */
    if (ppm < 2500) return 4; /* Poor */
    return 5;                 /* VeryPoor */
}

static void {{prefix_lc}}_scd_poll_cb(void *arg)
{
    if ({{prefix_lc}}_scd_send_cmd(0xEC05) != ESP_OK) return;
    vTaskDelay(pdMS_TO_TICKS(2));
    uint8_t buf[9] = {0};
    if (i2c_master_receive(s_{{prefix_lc}}_i2c_dev, buf, sizeof(buf), 50) != ESP_OK) return;
    uint16_t co2 = ((uint16_t)buf[0] << 8) | buf[1];
    if (co2 == 0) return; /* sensor still warming up */
    app_driver_param_val_t cv = { .u16 = co2 };
    app_driver_set_param({{cfg.co2_param}}, cv, APP_DRIVER_SOURCE_LOCAL);
    app_driver_param_val_t av = { .u8 = {{prefix_lc}}_aqi_from_co2(co2) };
    app_driver_set_param({{cfg.air_quality_param}}, av, APP_DRIVER_SOURCE_LOCAL);
}
