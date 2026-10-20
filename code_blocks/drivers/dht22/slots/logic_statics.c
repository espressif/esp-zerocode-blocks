/* {{prefix_lc}}: bit-banged DHT single-wire driver (self-contained). */

/* Busy-wait until the line reaches `level`, or timeout. Returns elapsed us,
 * or -1 on timeout. Elapsed time comes from esp_timer_get_time() (the
 * systimer, readable with interrupts masked): counting esp_rom_delay_us(1)
 * iterations under-counted every pulse by the loop's own overhead, which
 * pushed the 0/1 threshold around with the CPU clock. */
static inline int {{prefix_lc}}_dht_wait_level(int level, int timeout_us)
{
    int64_t t0 = esp_timer_get_time();
    for (;;) {
        int us = (int)(esp_timer_get_time() - t0);
        if (gpio_get_level((gpio_num_t){{prefix}}_DHT_GPIO) == level) return us;
        if (us > timeout_us) return -1;
    }
}

/* Read one 40-bit frame into out[5]. Returns ESP_OK on a valid checksum.
 * Timing-critical: the 40-bit read runs with interrupts masked. */
static esp_err_t {{prefix_lc}}_dht_read_frame(uint8_t out[5])
{
    gpio_num_t pin = (gpio_num_t){{prefix}}_DHT_GPIO;

    /* --- Start signal (interrupts still enabled; coarse ms-scale delays) --- */
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin, 0);
    /* DHT11 >=18 ms; DHT22 >=1 ms. The DHT11's long low sleeps (this is the
       block's own task); the DHT22's 2 ms is short enough to spin. */
    if ({{prefix}}_DHT_IS_DHT11) vTaskDelay(pdMS_TO_TICKS(20) + 1);   /* +1: a partial first tick never counts short */
    else esp_rom_delay_us(2000);
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

static uint32_t s_{{prefix_lc}}_dht_failures = 0;

static void {{prefix_lc}}_dht_poll(void)
{
    int16_t t = 0, h = 0;
    esp_err_t err = {{prefix_lc}}_dht_read(&t, &h);
    if (err == ESP_OK) {
        if (s_{{prefix_lc}}_dht_failures) {
            ESP_LOGI(TAG, "{{prefix_lc}}: DHT reading again after %lu failed poll(s)", (unsigned long)s_{{prefix_lc}}_dht_failures);
            s_{{prefix_lc}}_dht_failures = 0;
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
    /* First failure at WARN, then once a minute — a dead sensor used to WARN
       every poll. status_param (when composed) says "no data" instead of
       leaving a stale reading looking current. Same contract as aht21. */
    s_{{prefix_lc}}_dht_failures++;
    uint32_t per_minute = 60000UL / {{prefix}}_DHT_PERIOD_MS;
    if (per_minute == 0) per_minute = 1;
    if (s_{{prefix_lc}}_dht_failures == 1 || (s_{{prefix_lc}}_dht_failures % per_minute) == 0) {
        ESP_LOGW(TAG, "{{prefix_lc}}: DHT read failed (%s), %lu consecutive", esp_err_to_name(err), (unsigned long)s_{{prefix_lc}}_dht_failures);
    }
{{#if cfg.status_param}}
    app_driver_param_val_t bad = { .b = false };
    app_driver_set_param({{cfg.status_param}}, bad, APP_DRIVER_SOURCE_LOCAL);
{{/if}}
}

/* Its own task, not an esp_timer callback: the start pulse holds the line low
 * for 2 ms (20 ms on a DHT11) and the frame takes ~5 ms. On the shared
 * esp_timer task that delayed every other timer callback in the product. */
static void {{prefix_lc}}_dht_task(void *arg)
{
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        {{prefix_lc}}_dht_poll();
        vTaskDelayUntil(&last, pdMS_TO_TICKS({{prefix}}_DHT_PERIOD_MS));
    }
}
