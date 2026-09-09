/* {{prefix_lc}}: bit-banged DHT single-wire driver (self-contained). */

/* Busy-wait until the line reaches `level`, or timeout. Returns elapsed us,
 * or -1 on timeout. Uses esp_rom_delay_us — NOT the deprecated
 * <ets_sys.h>/ets_delay_us that broke the hand-vendored version. */
static inline int {{prefix_lc}}_dht_wait_level(int level, int timeout_us)
{
    int us = 0;
    while (gpio_get_level((gpio_num_t){{prefix}}_DHT_GPIO) != level) {
        if (us > timeout_us) return -1;
        esp_rom_delay_us(1);
        us++;
    }
    return us;
}

/* Read one 40-bit frame into out[5]. Returns ESP_OK on a valid checksum.
 * Timing-critical: the 40-bit read runs with interrupts masked. */
static esp_err_t {{prefix_lc}}_dht_read_frame(uint8_t out[5])
{
    gpio_num_t pin = (gpio_num_t){{prefix}}_DHT_GPIO;

    /* --- Start signal (interrupts still enabled; coarse ms-scale delays) --- */
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin, 0);
    esp_rom_delay_us({{prefix}}_DHT_IS_DHT11 ? 20000 : 2000);  /* DHT11 >=18 ms; DHT22 >=1 ms */
    gpio_set_level(pin, 1);
    esp_rom_delay_us(30);
    gpio_set_direction(pin, GPIO_MODE_INPUT);  /* external pull-up holds high */

    uint8_t bytes[5] = {0};

    /* --- Timing-critical section: mask interrupts around the 40-bit read.
     * A scheduler tick or ISR mid-frame would skew the us-scale pulse
     * measurements and corrupt the checksum. Critical section is short
     * (<5 ms) and contains no blocking calls. --- */
    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
    portENTER_CRITICAL(&mux);

    esp_err_t err = ESP_OK;
    do {
        /* Sensor response: ~80 us low, then ~80 us high, then first bit's low. */
        if ({{prefix_lc}}_dht_wait_level(0, 100) < 0) { err = ESP_ERR_TIMEOUT; break; }
        if ({{prefix_lc}}_dht_wait_level(1, 100) < 0) { err = ESP_ERR_TIMEOUT; break; }
        if ({{prefix_lc}}_dht_wait_level(0, 100) < 0) { err = ESP_ERR_TIMEOUT; break; }

        for (int i = 0; i < 40; i++) {
            /* Each bit: ~50 us low, then a high pulse (short=0, long=1). */
            if ({{prefix_lc}}_dht_wait_level(1, 80) < 0) { err = ESP_ERR_TIMEOUT; break; }
            int high_us = {{prefix_lc}}_dht_wait_level(0, 100);
            if (high_us < 0) { err = ESP_ERR_TIMEOUT; break; }
            /* >40 us high => logical 1 (0 pulse ~26-28 us, 1 pulse ~70 us). */
            if (high_us > 40) bytes[i / 8] |= (uint8_t)(1 << (7 - (i % 8)));
        }
    } while (0);

    portEXIT_CRITICAL(&mux);

    if (err != ESP_OK) return err;

    uint8_t sum = (uint8_t)(bytes[0] + bytes[1] + bytes[2] + bytes[3]);
    if (sum != bytes[4]) return ESP_ERR_INVALID_CRC;

    for (int i = 0; i < 5; i++) out[i] = bytes[i];
    return ESP_OK;
}

static esp_err_t {{prefix_lc}}_dht_read(int16_t *t_x100, int16_t *rh_x100)
{
    uint8_t b[5];
    esp_err_t err = {{prefix_lc}}_dht_read_frame(b);
    if (err != ESP_OK) return err;

    int32_t rh, t;
    if ({{prefix}}_DHT_IS_DHT11) {
        /* DHT11: integer bytes; fractional bytes (b[1]/b[3]) usually 0. */
        rh = (int32_t)b[0] * 100;   /* %RH * 100 */
        t  = (int32_t)b[2] * 100;   /* °C  * 100 */
    } else {
        /* DHT22/AM2301/DHT21: 16-bit, value/10. Temp high bit = sign. */
        uint16_t h_raw = ((uint16_t)b[0] << 8) | b[1];
        uint16_t t_raw = ((uint16_t)b[2] << 8) | b[3];
        rh = (int32_t)h_raw * 10;               /* (raw/10) * 100 */
        t  = (int32_t)(t_raw & 0x7FFF) * 10;     /* magnitude * 100/10 */
        if (t_raw & 0x8000) t = -t;              /* negative temperature */
    }

    if (rh < 0) rh = 0;
    if (rh > 10000) rh = 10000;
    if (t < -4000) t = -4000;   /* DHT operating range ~ -40..80 °C */
    if (t > 8000)  t = 8000;
    *rh_x100 = (int16_t)rh;
    *t_x100  = (int16_t)t;
    return ESP_OK;
}

static void {{prefix_lc}}_dht_poll_cb(void *arg)
{
    int16_t t = 0, h = 0;
    esp_err_t err = {{prefix_lc}}_dht_read(&t, &h);
    if (err == ESP_OK) {
        app_driver_param_val_t tv = { .i16 = t };
        app_driver_set_param({{cfg.temp_param}}, tv, APP_DRIVER_SOURCE_LOCAL);
        app_driver_param_val_t hv = { .i16 = h };
        app_driver_set_param({{cfg.humidity_param}}, hv, APP_DRIVER_SOURCE_LOCAL);
    } else {
        ESP_LOGW(TAG, "{{prefix_lc}}: DHT read failed: %s", esp_err_to_name(err));
    }
}
