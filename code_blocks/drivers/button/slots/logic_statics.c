static button_handle_t s_{{prefix_lc}}_button = NULL;

static void {{prefix_lc}}_toggle(void)
{
    app_driver_param_val_t val;
    app_driver_get_param({{cfg.target_param}}, &val);
    val.b = !val.b;
    ESP_LOGI(TAG, "Button {{prefix_lc}} pressed — toggling {{cfg.target_param}}");
    app_driver_set_param({{cfg.target_param}}, val, APP_DRIVER_SOURCE_LOCAL);
}

/* TOGGLE BEFORE THE RESET (toggle_before_reset, default on). The click alone
 * ends at the button's long-press time (1.5-2 s), so beside a 10 s factory-reset
 * hold on this pin (behaviors/factory_reset_long_press, which records its hold
 * on the shared device) a 3 s press toggled nothing. With a hold recorded, the
 * toggle runs at BUTTON_PRESS_END of a gesture of ONE press released before the
 * hold: for a click the moment SINGLE_CLICK fires, for a longer press its
 * release. A double press is never a toggle and a press that reaches the reset
 * never toggles. With no hold on the pin the click toggles, as always. */
#define {{prefix}}_TOGGLE_BEFORE_RESET ({{cfg.toggle_before_reset}})
static int64_t s_{{prefix_lc}}_down_us = 0;
static int64_t s_{{prefix_lc}}_press_ms = 0;
static int s_{{prefix_lc}}_presses = 0;

static int {{prefix_lc}}_reset_hold_ms(void)
{
    return {{prefix}}_TOGGLE_BEFORE_RESET ? app_button_reset_hold_ms(s_{{prefix_lc}}_button) : 0;
}
static void {{prefix_lc}}_press_down_cb(void *arg, void *usr_data)
{
    (void)arg; (void)usr_data;
    s_{{prefix_lc}}_presses++;
    s_{{prefix_lc}}_down_us = esp_timer_get_time();
{{#if cfg.press_param}}    /* Every press is one event, whatever gesture it becomes. */
    app_driver_param_val_t {{prefix_lc}}_pv = { .u8 = 1 };
    app_driver_fire_event({{cfg.press_param}}, {{prefix_lc}}_pv, APP_DRIVER_SOURCE_LOCAL);
{{/if}}}
static void {{prefix_lc}}_press_up_cb(void *arg, void *usr_data)
{
    (void)arg; (void)usr_data;
    s_{{prefix_lc}}_press_ms = (esp_timer_get_time() - s_{{prefix_lc}}_down_us) / 1000;
}
static void {{prefix_lc}}_press_end_cb(void *arg, void *usr_data)
{
    (void)arg; (void)usr_data;
    int presses = s_{{prefix_lc}}_presses;
    s_{{prefix_lc}}_presses = 0;
    int hold = {{prefix_lc}}_reset_hold_ms();
    if (hold > 0 && presses == 1 && s_{{prefix_lc}}_press_ms < hold) {{prefix_lc}}_toggle();
}

static void {{prefix_lc}}_button_cb(void *arg, void *usr_data)
{
    if ({{prefix_lc}}_reset_hold_ms() > 0) return;   /* the press end decides */
    {{prefix_lc}}_toggle();
}

/* WOKEN BY THIS BUTTON. A press that wakes the chip from deep sleep is usually
 * over before the button is created (the boot takes longer than a click), so
 * the button never saw it and the press that woke the device did nothing. When
 * this pin is what woke it (ext1 or the deep-sleep GPIO wakeup report the pin;
 * the classic ESP32's ext0 does not, so it is not attributed) and the button is
 * already released, that press is delivered once, as a click. A button still
 * held at boot is seen by the button itself. */
static bool {{prefix_lc}}_woke_device(void)
{
    if (esp_reset_reason() != ESP_RST_DEEPSLEEP) return false;
    uint32_t causes = esp_sleep_get_wakeup_causes();
#if SOC_PM_SUPPORT_EXT1_WAKEUP
    if ((causes & BIT(ESP_SLEEP_WAKEUP_EXT1)) &&
        (esp_sleep_get_ext1_wakeup_status() & (1ULL << {{prefix}}_BUTTON_GPIO))) return true;
#endif
#if SOC_GPIO_SUPPORT_HP_PERIPH_PD_SLEEP_WAKEUP
    if (causes & BIT(ESP_SLEEP_WAKEUP_GPIO)) {
        uint64_t st = esp_sleep_get_gpio_wakeup_status();
        for (uint32_t b = 0; b < 64; b++) {
            if (((st >> b) & 1ULL) && esp_sleep_wakeup_io_bit2num(b) == (gpio_num_t){{prefix}}_BUTTON_GPIO) return true;
        }
    }
#endif
    (void)causes;
    return false;
}
