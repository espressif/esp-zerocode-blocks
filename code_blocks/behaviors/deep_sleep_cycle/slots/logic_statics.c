/* {{prefix_lc}}: battery duty cycle — awake to report, then deep sleep. */
#if {{prefix}}_DSC_WAKE_GPIO < 0 && {{prefix}}_DSC_WAKE_GPIO_2 >= 0
#error "deep_sleep_cycle {{prefix}}: set wake_gpio before wake_gpio_2"
#endif
#if {{prefix}}_DSC_WAKE_GPIO < 0 && {{prefix}}_DSC_HEARTBEAT_MIN <= 0
#error "deep_sleep_cycle {{prefix}}: no wake source — set wake_gpio or heartbeat_minutes"
#endif
/* The pins each chip can wake from deep sleep (its RTC/LP IOs, as ESP-IDF's
 * rtc_io_channel.h / SOC_GPIO_HP_PERIPH_PD_SLEEP_WAKEABLE_MASK list them). Any
 * other pin is refused by every wake API, so the device would sleep through the
 * contact: that is a wiring mistake, stopped here rather than found in use. */
#if CONFIG_IDF_TARGET_ESP32
#define {{prefix}}_DSC_WAKEABLE 0xFF0E00F015ULL /* 0,2,4,12-15,25-27,32-39 */
#elif CONFIG_IDF_TARGET_ESP32C3
#define {{prefix}}_DSC_WAKEABLE 0x3FULL         /* 0-5 */
#elif CONFIG_IDF_TARGET_ESP32C5
#define {{prefix}}_DSC_WAKEABLE 0x7FULL         /* 0-6 */
#elif CONFIG_IDF_TARGET_ESP32C6 || CONFIG_IDF_TARGET_ESP32S31
#define {{prefix}}_DSC_WAKEABLE 0xFFULL         /* 0-7 */
#elif CONFIG_IDF_TARGET_ESP32H2
#define {{prefix}}_DSC_WAKEABLE 0x7F80ULL       /* 7-14 */
#elif CONFIG_IDF_TARGET_ESP32S3
#define {{prefix}}_DSC_WAKEABLE 0x3FFFFFULL     /* 0-21 */
#elif CONFIG_IDF_TARGET_ESP32P4
#define {{prefix}}_DSC_WAKEABLE 0xFFFFULL       /* 0-15 */
#endif
#ifdef {{prefix}}_DSC_WAKEABLE
#if {{prefix}}_DSC_WAKE_GPIO >= 0 && !((1ULL << {{prefix}}_DSC_WAKE_GPIO) & {{prefix}}_DSC_WAKEABLE)
#error "deep_sleep_cycle {{prefix}}: wake_gpio cannot wake this chip from deep sleep — move the contact to one of its RTC/LP pins (see block.yml)"
#endif
#if {{prefix}}_DSC_WAKE_GPIO_2 >= 0 && !((1ULL << {{prefix}}_DSC_WAKE_GPIO_2) & {{prefix}}_DSC_WAKEABLE)
#error "deep_sleep_cycle {{prefix}}: wake_gpio_2 cannot wake this chip from deep sleep — move the contact to one of its RTC/LP pins (see block.yml)"
#endif
#endif

/* Defined by this block's mqtt_* slots in an MQTT product; null otherwise. */
extern "C" int {{prefix_lc}}_dsc_net_state(void) __attribute__((weak));
extern "C" void {{prefix_lc}}_dsc_net_quiet(void) __attribute__((weak));

static const int s_{{prefix_lc}}_dsc_pins[2] = { {{prefix}}_DSC_WAKE_GPIO, {{prefix}}_DSC_WAKE_GPIO_2 };
static bool s_{{prefix_lc}}_dsc_deep_wake = false;

/* Bit i = current level of wake pin i. */
static int {{prefix_lc}}_dsc_levels(void)
{
    int bits = 0;
    for (int i = 0; i < 2; i++) {
        int pin = s_{{prefix_lc}}_dsc_pins[i];
        if (pin >= 0 && gpio_get_level((gpio_num_t)pin)) bits |= 1 << i;
    }
    return bits;
}

static bool {{prefix_lc}}_dsc_usb_connected(void)
{
#if {{prefix}}_DSC_USB_AWAKE && SOC_USB_SERIAL_JTAG_SUPPORTED && CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG_ENABLED
    return usb_serial_jtag_is_connected();
#else
    return false;
#endif
}

/* The pull held on a wake pin through deep sleep. */
static void {{prefix_lc}}_dsc_pull(gpio_num_t pin)
{
#if SOC_RTCIO_INPUT_OUTPUT_SUPPORTED
    if (rtc_gpio_is_valid_gpio(pin)) {
#if defined({{prefix}}_DSC_PULL_pullup)
        rtc_gpio_pulldown_dis(pin);
        rtc_gpio_pullup_en(pin);
#elif defined({{prefix}}_DSC_PULL_pulldown)
        rtc_gpio_pullup_dis(pin);
        rtc_gpio_pulldown_en(pin);
#else
        rtc_gpio_pullup_dis(pin);
        rtc_gpio_pulldown_dis(pin);
#endif
        return;
    }
#endif
#if defined({{prefix}}_DSC_PULL_pullup)
    gpio_pulldown_dis(pin);
    gpio_pullup_en(pin);
#elif defined({{prefix}}_DSC_PULL_pulldown)
    gpio_pullup_dis(pin);
    gpio_pulldown_en(pin);
#else
    gpio_pullup_dis(pin);
    gpio_pulldown_dis(pin);
#endif
}

/* Wake when pin goes to the level it is NOT at now: level wake on every chip,
 * re-armed each sleep, so both opening and closing wake it, and a change that
 * lands while going to sleep wakes it at once instead of being missed. */
static bool {{prefix_lc}}_dsc_arm_pin(int idx, gpio_num_t pin, int level)
{
    esp_err_t err = ESP_ERR_NOT_SUPPORTED;
    {{prefix_lc}}_dsc_pull(pin);
#if SOC_PM_SUPPORT_EXT1_WAKEUP_MODE_PER_PIN
    (void)idx;
    err = esp_sleep_enable_ext1_wakeup_io(1ULL << pin, level ? ESP_EXT1_WAKEUP_ANY_LOW : ESP_EXT1_WAKEUP_ANY_HIGH);
#elif SOC_PM_SUPPORT_EXT1_WAKEUP
    /* One ext1 level for all its pins here: the second pin goes on ext0. */
    if (idx == 0) {
#if CONFIG_IDF_TARGET_ESP32
        err = esp_sleep_enable_ext1_wakeup_io(1ULL << pin, level ? ESP_EXT1_WAKEUP_ALL_LOW : ESP_EXT1_WAKEUP_ANY_HIGH);
#else
        err = esp_sleep_enable_ext1_wakeup_io(1ULL << pin, level ? ESP_EXT1_WAKEUP_ANY_LOW : ESP_EXT1_WAKEUP_ANY_HIGH);
#endif
    } else {
#if SOC_PM_SUPPORT_EXT0_WAKEUP
        err = esp_sleep_enable_ext0_wakeup(pin, level ? 0 : 1);
#endif
    }
#elif SOC_GPIO_SUPPORT_HP_PERIPH_PD_SLEEP_WAKEUP
    (void)idx;
    err = esp_sleep_enable_gpio_wakeup_on_hp_periph_powerdown(1ULL << pin,
              level ? ESP_GPIO_WAKEUP_GPIO_LOW : ESP_GPIO_WAKEUP_GPIO_HIGH);
#else
    (void)idx;
#endif
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "{{prefix_lc}}: GPIO %d cannot wake this chip from deep sleep (%s)", (int)pin, esp_err_to_name(err));
        return false;
    }
    ESP_LOGI(TAG, "{{prefix_lc}}: wake when GPIO %d goes %s", (int)pin, level ? "low" : "high");
    return true;
}

static void {{prefix_lc}}_dsc_sleep(const char *why)
{
    if ({{prefix_lc}}_dsc_net_quiet) {
        {{prefix_lc}}_dsc_net_quiet();
        vTaskDelay(pdMS_TO_TICKS(200));   /* let the DISCONNECT leave */
    }
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    int levels = {{prefix_lc}}_dsc_levels();
    bool armed = false;
    for (int i = 0; i < 2; i++) {
        int pin = s_{{prefix_lc}}_dsc_pins[i];
        if (pin < 0) continue;
        armed |= {{prefix_lc}}_dsc_arm_pin(i, (gpio_num_t)pin, (levels >> i) & 1);
    }
    uint64_t minutes = {{prefix}}_DSC_HEARTBEAT_MIN > 0 ? (uint64_t){{prefix}}_DSC_HEARTBEAT_MIN : 0;
    if (!armed && minutes == 0) {
        minutes = 60;   /* never sleep with nothing to wake it */
        ESP_LOGE(TAG, "{{prefix_lc}}: no pin could be armed — waking hourly instead");
    }
    if (minutes > 0) esp_sleep_enable_timer_wakeup(minutes * 60ULL * 1000000ULL);
    ESP_LOGI(TAG, "{{prefix_lc}}: deep sleep (%s), awake %lld ms", why, (long long)(esp_timer_get_time() / 1000));
    esp_deep_sleep_start();
}

static void {{prefix_lc}}_dsc_task(void *arg)
{
    (void)arg;
    int64_t awake_from = 0;
    int64_t last_change = 0;
    int last_levels = {{prefix_lc}}_dsc_levels();
    bool told_unconfigured = false;
{{#if cfg.state_param}}    bool have_state = false;
    bool last_state = false;
{{/if}}    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(100));
        int64_t now = esp_timer_get_time() / 1000;
        int levels = {{prefix_lc}}_dsc_levels();
        if (levels != last_levels) {
            last_levels = levels;
            last_change = now;
        }
        bool settled = now - last_change >= {{prefix}}_DSC_QUIET_MS;
{{#if cfg.state_param}}        app_driver_param_val_t sv = {};
        if (app_driver_param_seen({{cfg.state_param}}) &&
            app_driver_get_param({{cfg.state_param}}, &sv) == ESP_OK &&
            (!have_state || sv.b != last_state)) {
            have_state = true;
            last_state = sv.b;
            last_change = now;
            settled = false;
        }
        settled = settled && have_state;
{{/if}}
        if ({{prefix_lc}}_dsc_usb_connected()) {
            awake_from = now;   /* on a computer: stay up, restart the clock */
            continue;
        }
        if (!s_{{prefix_lc}}_dsc_deep_wake && now < (int64_t){{prefix}}_DSC_BOOT_AWAKE_S * 1000) continue;

        int net = {{prefix_lc}}_dsc_net_state ? {{prefix_lc}}_dsc_net_state() : -1;
        if (net == {{prefix}}_DSC_NET_UNCONFIGURED) {
            if (!told_unconfigured) {
                ESP_LOGW(TAG, "{{prefix_lc}}: network not set up — staying awake until it is");
                told_unconfigured = true;
            }
            awake_from = now;
            continue;
        }
        if (net == {{prefix}}_DSC_NET_REPORTED && settled) {{prefix_lc}}_dsc_sleep("state reported");
        if (now - awake_from >= {{prefix}}_DSC_MAX_AWAKE_MS) {
            {{prefix_lc}}_dsc_sleep(net == {{prefix}}_DSC_NET_PENDING ? "max awake, not reported" : "max awake");
        }
    }
}
