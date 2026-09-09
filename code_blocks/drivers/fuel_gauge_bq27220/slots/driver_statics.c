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
    esp_err_t {{prefix_lc}}_err = i2c_master_get_bus_handle((i2c_port_num_t){{prefix}}_GAUGE_PORT, &{{prefix_lc}}_bus);
    if ({{prefix_lc}}_err != ESP_OK) {
        /* Fail CLOSED, and say which port. Aborting would take a whole product
         * down for one absent sensor; staying quiet is how a device ends up
         * silent with nothing in any report saying why. */
        ESP_LOGE(TAG, "{{prefix_lc}}: no I2C bus on port %d (%s) — needs a bus provider EARLIER in the product, or a board that owns the port",
                 (int){{prefix}}_GAUGE_PORT, esp_err_to_name({{prefix_lc}}_err));
        return {{prefix_lc}}_err;
    }
    /* Field-by-field, not a designated-initializer list: these files compile as
     * C++, where initializer order is fixed and a nested flags struct makes the
     * positional form brittle. SCL speed is per DEVICE under this driver. */
    i2c_device_config_t {{prefix_lc}}_dev_cfg = {};
    {{prefix_lc}}_dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    {{prefix_lc}}_dev_cfg.device_address = {{prefix}}_GAUGE_ADDR;
    {{prefix_lc}}_dev_cfg.scl_speed_hz = {{prefix}}_GAUGE_FREQ_HZ;
    {{prefix_lc}}_err = i2c_master_bus_add_device({{prefix_lc}}_bus, &{{prefix_lc}}_dev_cfg, &s_{{prefix_lc}}_i2c_dev);
    if ({{prefix_lc}}_err != ESP_OK) {
        ESP_LOGE(TAG, "{{prefix_lc}}: i2c_master_bus_add_device(0x%02x): %s",
                 (unsigned){{prefix}}_GAUGE_ADDR, esp_err_to_name({{prefix_lc}}_err));
        s_{{prefix_lc}}_i2c_dev = NULL;
    }
    return {{prefix_lc}}_err;
}

/* {{prefix_lc}}: BQ27220 fuel gauge — read-only, see block.yml. */

static bool s_{{prefix_lc}}_present = false;

/* Standard commands are 16-bit little-endian. */
static esp_err_t {{prefix_lc}}_rd16(uint8_t reg, uint16_t *out)
{
    uint8_t b[2] = {0};
    esp_err_t err = i2c_master_transmit_receive(s_{{prefix_lc}}_i2c_dev, &reg, 1, b, sizeof(b), 50);
    if (err != ESP_OK) return err;
    *out = (uint16_t)b[0] | ((uint16_t)b[1] << 8);
    return ESP_OK;
}

static void {{prefix_lc}}_poll_cb(void *arg)
{
    (void)arg;
    if (!s_{{prefix_lc}}_present) return;

    uint16_t soc = 0;
    if ({{prefix_lc}}_rd16({{prefix}}_GAUGE_REG_SOC, &soc) != ESP_OK) return;
    /* The gauge can report >100 briefly just after a full charge. Clamping
       here keeps every consumer from having to know that. */
    if (soc > 100) soc = 100;

    app_driver_param_val_t v = { .u8 = (uint8_t)soc };
    app_driver_set_param({{cfg.soc_param}}, v, APP_DRIVER_SOURCE_LOCAL);
{{#if cfg.voltage_param}}
    {
        uint16_t mv = 0;
        if ({{prefix_lc}}_rd16({{prefix}}_GAUGE_REG_VOLTAGE, &mv) == ESP_OK) {
            app_driver_param_val_t vv = { .u16 = mv };
            app_driver_set_param({{cfg.voltage_param}}, vv, APP_DRIVER_SOURCE_LOCAL);
        }
    }
{{/if}}
{{#if cfg.low_battery_param}}
    {
        /* Edge-triggered: a low-battery flag that rewrites itself every poll
           would wake any solution subscribed to it twice a minute forever. */
        static int last_low = -1;
        int low = (soc <= {{prefix}}_GAUGE_LOW_PCT) ? 1 : 0;
        if (low != last_low) {
            last_low = low;
            app_driver_param_val_t lv = { .b = low != 0 };
            app_driver_set_param({{cfg.low_battery_param}}, lv, APP_DRIVER_SOURCE_LOCAL);
            ESP_LOGI(TAG, "{{prefix_lc}}: battery %s (%u%%)", low ? "LOW" : "ok", (unsigned)soc);
        }
    }
{{/if}}
}
