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
    esp_err_t {{prefix_lc}}_err = i2c_master_get_bus_handle((i2c_port_num_t){{prefix}}_SHT3_PORT, &{{prefix_lc}}_bus);
    if ({{prefix_lc}}_err != ESP_OK) {
        /* Fail CLOSED, and say which port. Aborting would take a whole product
         * down for one absent sensor; staying quiet is how a device ends up
         * silent with nothing in any report saying why. */
        ESP_LOGE(TAG, "{{prefix_lc}}: no I2C bus on port %d (%s) — needs a bus provider EARLIER in the product, or a board that owns the port",
                 (int){{prefix}}_SHT3_PORT, esp_err_to_name({{prefix_lc}}_err));
        return {{prefix_lc}}_err;
    }
    /* Field-by-field, not a designated-initializer list: these files compile as
     * C++, where initializer order is fixed and a nested flags struct makes the
     * positional form brittle. SCL speed is per DEVICE under this driver. */
    i2c_device_config_t {{prefix_lc}}_dev_cfg = {};
    {{prefix_lc}}_dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    {{prefix_lc}}_dev_cfg.device_address = {{prefix}}_SHT3_ADDR;
    {{prefix_lc}}_dev_cfg.scl_speed_hz = {{prefix}}_SHT3_FREQ_HZ;
    {{prefix_lc}}_err = i2c_master_bus_add_device({{prefix_lc}}_bus, &{{prefix_lc}}_dev_cfg, &s_{{prefix_lc}}_i2c_dev);
    if ({{prefix_lc}}_err != ESP_OK) {
        ESP_LOGE(TAG, "{{prefix_lc}}: i2c_master_bus_add_device(0x%02x): %s",
                 (unsigned){{prefix}}_SHT3_ADDR, esp_err_to_name({{prefix_lc}}_err));
        s_{{prefix_lc}}_i2c_dev = NULL;
    }
    return {{prefix_lc}}_err;
}

static esp_err_t {{prefix_lc}}_sht3_read(int16_t *t_x100, int16_t *rh_x100)
{
    /* 0x2400 = single shot, high repeatability, NO clock stretching. The
       stretching variant (0x2C06) holds SCL low until the measurement is done
       — up to 15 ms — and the I2C master gives up on a stretched clock after
       its default scl_wait_us (~2 ms, I2C_LL_SCL_WAIT_US_VAL_DEFAULT), so an
       early read timed out. Without stretching an early read is a plain NACK,
       and the wait below makes it not early. */
    uint8_t cmd[2] = { 0x24, 0x00 };
    esp_err_t err = i2c_master_transmit(s_{{prefix_lc}}_i2c_dev, cmd, sizeof(cmd), 50);
    if (err != ESP_OK) return err;
    /* +1 tick: vTaskDelay(n) waits (n-1, n] ticks, and at the default 100 Hz
       pdMS_TO_TICKS(15) is ONE tick (0-10 ms) against a 15 ms maximum
       measurement time. One extra tick floors the wait at 15 ms. */
    vTaskDelay(pdMS_TO_TICKS(15) + 1);
    uint8_t buf[6] = {0};
    err = i2c_master_receive(s_{{prefix_lc}}_i2c_dev, buf, sizeof(buf), 50);
    if (err != ESP_OK) return err;
    uint16_t t_raw = ((uint16_t)buf[0] << 8) | buf[1];
    uint16_t h_raw = ((uint16_t)buf[3] << 8) | buf[4];
    int32_t t = -4500 + (int32_t)((17500LL * (int32_t)t_raw) / 65535);
    int32_t h =          (int32_t)((10000LL * (int32_t)h_raw) / 65535);
    if (h < 0) h = 0;
    if (h > 10000) h = 10000;
    *t_x100  = (int16_t)t;
    *rh_x100 = (int16_t)h;
    return ESP_OK;
}

static void {{prefix_lc}}_sht3_poll_cb(void *arg)
{
    int16_t t = 0, h = 0;
    esp_err_t err = {{prefix_lc}}_sht3_read(&t, &h);
    if (err == ESP_OK) {
        app_driver_param_val_t tv = { .i16 = t };
        app_driver_set_param({{cfg.temp_param}}, tv, APP_DRIVER_SOURCE_LOCAL);
        app_driver_param_val_t hv = { .i16 = h };
        app_driver_set_param({{cfg.humidity_param}}, hv, APP_DRIVER_SOURCE_LOCAL);
    } else {
        /* Say so: a failed read used to return silently, so a dead or
           miswired sensor left nothing in the device log. */
        ESP_LOGW(TAG, "{{prefix_lc}}: SHT3x read failed: %s", esp_err_to_name(err));
    }
}
