static button_handle_t s_{{prefix_lc}}_btn = NULL;

/* Set by the default long-press hook: a long press nobody handles is still a
 * press, and released before a factory reset on this pin it toggles. */
static bool s_{{prefix_lc}}_lp_default = false;

__attribute__((weak)) void {{prefix_lc}}_on_double_click(void)
{
    ESP_LOGI(TAG, "{{prefix_lc}}: double-click (no handler installed)");
}
__attribute__((weak)) void {{prefix_lc}}_on_long_press(void)
{
    s_{{prefix_lc}}_lp_default = true;
    ESP_LOGI(TAG, "{{prefix_lc}}: long-press (no handler installed)");
}

static void {{prefix_lc}}_toggle(void)
{
    app_driver_param_val_t v;
    app_driver_get_param({{cfg.target_param}}, &v);
    v.b = !v.b;
    ESP_LOGI(TAG, "{{prefix_lc}}: single-click — toggling {{cfg.target_param}}");
    app_driver_set_param({{cfg.target_param}}, v, APP_DRIVER_SOURCE_LOCAL);
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
static bool s_{{prefix_lc}}_lp_taken = false;   /* the product's long-press hook ran in this gesture */

static int {{prefix_lc}}_reset_hold_ms(void)
{
    return {{prefix}}_TOGGLE_BEFORE_RESET ? app_button_reset_hold_ms(s_{{prefix_lc}}_btn) : 0;
}
static void {{prefix_lc}}_press_down_cb(void *arg, void *usr_data)
{
    (void)arg; (void)usr_data;
    if (s_{{prefix_lc}}_presses == 0) s_{{prefix_lc}}_lp_taken = false;
    s_{{prefix_lc}}_presses++;
    s_{{prefix_lc}}_down_us = esp_timer_get_time();
}
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
    if (hold > 0 && presses == 1 && s_{{prefix_lc}}_press_ms < hold && !s_{{prefix_lc}}_lp_taken) {{prefix_lc}}_toggle();
}

static void {{prefix_lc}}_single_click_cb(void *arg, void *usr_data)
{
    if ({{prefix_lc}}_reset_hold_ms() > 0) return;   /* the press end decides */
    {{prefix_lc}}_toggle();
}
static void {{prefix_lc}}_double_click_cb(void *arg, void *usr_data)  { {{prefix_lc}}_on_double_click(); }
static void {{prefix_lc}}_long_press_cb (void *arg, void *usr_data)
{
    s_{{prefix_lc}}_lp_default = false;
    {{prefix_lc}}_on_long_press();
    if (!s_{{prefix_lc}}_lp_default) s_{{prefix_lc}}_lp_taken = true;
}
