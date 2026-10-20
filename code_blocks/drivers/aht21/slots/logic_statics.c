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
    esp_err_t {{prefix_lc}}_err = i2c_master_get_bus_handle((i2c_port_num_t){{prefix}}_AHT_PORT, &{{prefix_lc}}_bus);
    if ({{prefix_lc}}_err != ESP_OK) {
        /* Fail CLOSED, and say which port. Aborting would take a whole product
         * down for one absent sensor; staying quiet is how a device ends up
         * silent with nothing in any report saying why. */
        ESP_LOGE(TAG, "{{prefix_lc}}: no I2C bus on port %d (%s) — needs a bus provider EARLIER in the product, or a board that owns the port",
                 (int){{prefix}}_AHT_PORT, esp_err_to_name({{prefix_lc}}_err));
        return {{prefix_lc}}_err;
    }
    /* Field-by-field, not a designated-initializer list: these files compile as
     * C++, where initializer order is fixed and a nested flags struct makes the
     * positional form brittle. SCL speed is per DEVICE under this driver. */
    i2c_device_config_t {{prefix_lc}}_dev_cfg = {};
    {{prefix_lc}}_dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    {{prefix_lc}}_dev_cfg.device_address = {{prefix}}_AHT_ADDR;
    {{prefix_lc}}_dev_cfg.scl_speed_hz = {{prefix}}_AHT_FREQ_HZ;
    {{prefix_lc}}_err = i2c_master_bus_add_device({{prefix_lc}}_bus, &{{prefix_lc}}_dev_cfg, &s_{{prefix_lc}}_i2c_dev);
    if ({{prefix_lc}}_err != ESP_OK) {
        ESP_LOGE(TAG, "{{prefix_lc}}: i2c_master_bus_add_device(0x%02x): %s",
                 (unsigned){{prefix}}_AHT_ADDR, esp_err_to_name({{prefix_lc}}_err));
        s_{{prefix_lc}}_i2c_dev = NULL;
    }
    return {{prefix_lc}}_err;
}

/* Power-on calibration check (AHT20/AHT21 datasheets): read the status byte
 * and, if the calibration-enable bit (0x08) is clear, send the init command
 * 0xBE 0x08 0x00 and give it 10 ms. A part that skipped it returns readings
 * from an uncalibrated front end — plausible numbers, wrong ones. Runs once,
 * from logic_init (app_main's task), never from a timer callback. */
static void {{prefix_lc}}_aht_calibrate(void)
{
    vTaskDelay(pdMS_TO_TICKS(40));   /* >= 40 ms after power-on before the first command */
    uint8_t st = 0;
    if (i2c_master_receive(s_{{prefix_lc}}_i2c_dev, &st, 1, 50) != ESP_OK) return;  /* the poll will report it */
    if (st & 0x08) return;
    uint8_t init[3] = { 0xBE, 0x08, 0x00 };
    if (i2c_master_transmit(s_{{prefix_lc}}_i2c_dev, init, sizeof(init), 50) == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(10));
        ESP_LOGI(TAG, "AHT21 {{prefix_lc}}: calibration bit was clear — sent init");
    }
}

/* CRC-8 over the 6 data bytes: polynomial 0x31, init 0xFF (datasheet). */
static uint8_t {{prefix_lc}}_aht_crc8(const uint8_t *d, int n)
{
    uint8_t crc = 0xFF;
    for (int i = 0; i < n; i++) {
        crc ^= d[i];
        for (int b = 0; b < 8; b++) crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
    }
    return crc;
}

/* The measurement is SPLIT across two timer callbacks: the poll sends the
 * trigger, and a one-shot timer reads the result ~80 ms later. Waiting the
 * conversion out inside the poll callback (vTaskDelay(80)) stalled every other
 * esp_timer user in the product — button scanning, LED patterns, other
 * sensors — for 80 ms every poll. */
static esp_err_t {{prefix_lc}}_aht_trigger(void)
{
    uint8_t trig[3] = { 0xAC, 0x33, 0x00 };
    return i2c_master_transmit(s_{{prefix_lc}}_i2c_dev, trig, sizeof(trig), 50);
}

static esp_err_t {{prefix_lc}}_aht_fetch(int16_t *t_x100, int16_t *rh_x100)
{
    uint8_t buf[7] = {0};
    esp_err_t err = i2c_master_receive(s_{{prefix_lc}}_i2c_dev, buf, sizeof(buf), 50);
    if (err != ESP_OK) return err;
    if (buf[0] & 0x80) return ESP_ERR_NOT_FINISHED; /* still busy */
    /* A corrupted frame publishes as a real reading unless the CRC says no. */
    if ({{prefix_lc}}_aht_crc8(buf, 6) != buf[6]) return ESP_ERR_INVALID_CRC;
    uint32_t h_raw = ((uint32_t)buf[1] << 12) | ((uint32_t)buf[2] << 4) | ((uint32_t)buf[3] >> 4);
    uint32_t t_raw = (((uint32_t)buf[3] & 0x0F) << 16) | ((uint32_t)buf[4] << 8) | (uint32_t)buf[5];
    int32_t t = -5000 + (int32_t)((20000LL * (int32_t)t_raw) / 0xFFFFF);
    int32_t h =          (int32_t)((10000LL * (int32_t)h_raw) / 0xFFFFF);
    if (h < 0) h = 0;
    if (h > 10000) h = 10000;
    *t_x100  = (int16_t)t;
    *rh_x100 = (int16_t)h;
    return ESP_OK;
}

static uint32_t s_{{prefix_lc}}_aht_failures = 0;

static esp_timer_handle_t s_{{prefix_lc}}_aht_read_timer = NULL;

static void {{prefix_lc}}_aht_report(esp_err_t err, int16_t t, int16_t h);

/* One-shot, ~80 ms after the trigger: fetch and report. */
static void {{prefix_lc}}_aht_read_cb(void *arg)
{
    int16_t t = 0, h = 0;
    esp_err_t err = {{prefix_lc}}_aht_fetch(&t, &h);
    {{prefix_lc}}_aht_report(err, t, h);
}

static void {{prefix_lc}}_aht_poll_cb(void *arg)
{
    esp_err_t err = {{prefix_lc}}_aht_trigger();
    if (err == ESP_OK && s_{{prefix_lc}}_aht_read_timer) {
        err = esp_timer_start_once(s_{{prefix_lc}}_aht_read_timer, 80 * 1000ULL);
        if (err == ESP_OK) return;   /* the read callback reports */
    }
    {{prefix_lc}}_aht_report(err, 0, 0);
}

static void {{prefix_lc}}_aht_report(esp_err_t err, int16_t t, int16_t h)
{
    if (err == ESP_OK) {
        if (s_{{prefix_lc}}_aht_failures) {
            ESP_LOGI(TAG, "AHT21 {{prefix_lc}}: reading again after %lu failed poll(s)", (unsigned long)s_{{prefix_lc}}_aht_failures);
            s_{{prefix_lc}}_aht_failures = 0;
        }
        app_driver_param_val_t tv = { .i16 = t };
        app_driver_set_param({{cfg.temp_param}}, tv, APP_DRIVER_SOURCE_LOCAL);
        app_driver_param_val_t hv = { .i16 = h };
        app_driver_set_param({{cfg.humidity_param}}, hv, APP_DRIVER_SOURCE_LOCAL);
{{#if cfg.status_param}}
        app_driver_param_val_t ok = { .b = true };
        app_driver_set_param({{cfg.status_param}}, ok, APP_DRIVER_SOURCE_LOCAL);
{{/if}}
        return;
    }
    /* A failed read leaves the last values on the bus — they are still the
     * best reading there is — but says so: the first failure at WARN, then
     * once a minute, so a dead sensor is visible in the log without flooding
     * it. status_param (when composed) tells displays and publishers to show
     * "no data" instead of a stale number. */
    s_{{prefix_lc}}_aht_failures++;
    uint32_t per_minute = 60000UL / (uint32_t){{prefix}}_AHT_POLL_MS;
    if (per_minute == 0) per_minute = 1;
    if (s_{{prefix_lc}}_aht_failures == 1 || (s_{{prefix_lc}}_aht_failures % per_minute) == 0) {
        ESP_LOGW(TAG, "AHT21 {{prefix_lc}}: read failed (%s), %lu consecutive", esp_err_to_name(err), (unsigned long)s_{{prefix_lc}}_aht_failures);
    }
{{#if cfg.status_param}}
    app_driver_param_val_t bad = { .b = false };
    app_driver_set_param({{cfg.status_param}}, bad, APP_DRIVER_SOURCE_LOCAL);
{{/if}}
}
