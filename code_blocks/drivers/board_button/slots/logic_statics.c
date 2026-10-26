/* {{prefix_lc}}: the board's own '{{cfg.device}}' button device.
 * No GPIO number appears anywhere in this product — the pin and active level
 * are the BOARD's facts, and claiming the pin here would be an IO conflict
 * with the board's own peripheral. dev_button is built ON iot_button (its
 * handles ARE button_handle_t), so these are ordinary iot_button callbacks on
 * a device somebody else created.
 *
 * Gesture hooks: weak, so the product overrides any of them with a plain
 * definition of the same signature in its own file. They run on the button
 * timer task — keep them short and non-blocking. */
{{#if cfg.target_param}}#define {{prefix}}_SINGLE_CLICK_TOGGLES 1
{{/if}}/* Set by the default long-press hook: a long press nobody handles is still a
 * press, and released before the factory reset it toggles (see below). */
static bool s_{{prefix_lc}}_lp_default = false;
__attribute__((weak)) void {{prefix_lc}}_on_single_click(void)
{
#ifndef {{prefix}}_SINGLE_CLICK_TOGGLES
    ESP_LOGI(TAG, "{{prefix_lc}}: single click (no handler)");
#endif
    /* With target_param bound the click already toggled it — nothing more. */
}
__attribute__((weak)) void {{prefix_lc}}_on_double_click(void)
{
    ESP_LOGI(TAG, "{{prefix_lc}}: double click (no handler)");
}
__attribute__((weak)) void {{prefix_lc}}_on_long_press(void)
{
    s_{{prefix_lc}}_lp_default = true;
    ESP_LOGI(TAG, "{{prefix_lc}}: long press (no handler)");
}

/* TOGGLE BEFORE THE RESET (toggle_before_reset, with target_param and
 * a reset hold). The click alone ends at the board's long-press time
 * (1.5-2 s), so a 3 s press on a button with a 10 s reset hold toggled
 * nothing. In this mode the toggle (and the single-click hook) runs at
 * BUTTON_PRESS_END of a gesture of ONE press that was released before the
 * reset: for a click that is the same moment SINGLE_CLICK fires (after the
 * double-click window), for a longer press its release. A double press stays a
 * double click, a press that reaches the reset never toggles, and a press the
 * product's own long-press hook handled is that gesture, not a toggle. */
#define {{prefix}}_TOGGLE_BEFORE_RESET 0
{{#if cfg.target_param}}#undef {{prefix}}_TOGGLE_BEFORE_RESET
#define {{prefix}}_TOGGLE_BEFORE_RESET ({{cfg.toggle_before_reset}} && {{cfg.factory_reset_ms}} > 0)
{{/if}}static int64_t s_{{prefix_lc}}_down_us = 0;
static int64_t s_{{prefix_lc}}_press_ms = 0;
static int s_{{prefix_lc}}_presses = 0;
static bool s_{{prefix_lc}}_lp_taken = false;

static void {{prefix_lc}}_click(void)
{
{{#if cfg.target_param}}    app_driver_param_val_t val;
    app_driver_get_param({{cfg.target_param}}, &val);
    val.b = !val.b;
    ESP_LOGI(TAG, "{{prefix_lc}}: board button '{{cfg.device}}' pressed — toggling {{cfg.target_param}}");
    app_driver_set_param({{cfg.target_param}}, val, APP_DRIVER_SOURCE_LOCAL);
{{/if}}    {{prefix_lc}}_on_single_click();
}
static void {{prefix_lc}}_board_button_cb(void *arg, void *usr_data)
{
    (void)arg; (void)usr_data;
    if ({{prefix}}_TOGGLE_BEFORE_RESET) return;   /* the press end decides */
    {{prefix_lc}}_click();
}
static void {{prefix_lc}}_press_down_cb(void *arg, void *usr_data)
{
    (void)arg; (void)usr_data;
    if (s_{{prefix_lc}}_presses++ == 0) s_{{prefix_lc}}_lp_taken = false;
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
    if (!{{prefix}}_TOGGLE_BEFORE_RESET) return;
    if (presses == 1 && s_{{prefix_lc}}_press_ms < {{cfg.factory_reset_ms}} && !s_{{prefix_lc}}_lp_taken) {
        {{prefix_lc}}_click();
    }
}
static void {{prefix_lc}}_double_click_cb(void *arg, void *usr_data) { (void)arg; (void)usr_data; {{prefix_lc}}_on_double_click(); }
static void {{prefix_lc}}_long_press_cb(void *arg, void *usr_data)
{
    (void)arg; (void)usr_data;
    s_{{prefix_lc}}_lp_default = false;
    {{prefix_lc}}_on_long_press();
    if (!s_{{prefix_lc}}_lp_default) s_{{prefix_lc}}_lp_taken = true;
}
{{#if cfg.factory_reset_ms}}
/* Factory-reset hold feedback: app_factory_reset_hold_started() once the press
 * has lasted the cue (a one-shot esp_timer from press-down — iot_button never
 * fires a per-callback long press shorter than the DEVICE's long-press time,
 * which is the board's), app_factory_reset_hold_cancelled() on a release
 * before the reset. Everything here runs on the esp_timer task, as the button
 * callbacks do, so the two flags need no lock. */
#define {{prefix}}_FR_HOLD_MS {{cfg.factory_reset_ms}}
#define {{prefix}}_FR_CUE_CFG_MS {{cfg.reset_cue_ms}}
#define {{prefix}}_FR_CUE_MS ({{prefix}}_FR_CUE_CFG_MS > 0 ? {{prefix}}_FR_CUE_CFG_MS : ({{cfg.long_press_ms}} < {{prefix}}_FR_HOLD_MS ? {{cfg.long_press_ms}} : 1000))
static esp_timer_handle_t s_{{prefix_lc}}_fr_cue = NULL;
static bool s_{{prefix_lc}}_fr_held = false;   /* started was called for this press */
static bool s_{{prefix_lc}}_fr_fired = false;  /* the reset fired during this press */

static void {{prefix_lc}}_fr_cue_cb(void *arg)
{
    (void)arg;
    if (s_{{prefix_lc}}_fr_held || s_{{prefix_lc}}_fr_fired) return;
    s_{{prefix_lc}}_fr_held = true;
    ESP_LOGI(TAG, "{{prefix_lc}}: held %d ms — factory reset at %d ms unless released", (int){{prefix}}_FR_CUE_MS, (int){{prefix}}_FR_HOLD_MS);
    app_factory_reset_hold_started();
}
static void {{prefix_lc}}_fr_down_cb(void *arg, void *usr_data)
{
    (void)arg; (void)usr_data;
    s_{{prefix_lc}}_fr_held = false;
    s_{{prefix_lc}}_fr_fired = false;
    if (s_{{prefix_lc}}_fr_cue) {
        esp_timer_stop(s_{{prefix_lc}}_fr_cue);
        esp_timer_start_once(s_{{prefix_lc}}_fr_cue, (uint64_t){{prefix}}_FR_CUE_MS * 1000ULL);
    }
}
static void {{prefix_lc}}_fr_up_cb(void *arg, void *usr_data)
{
    (void)arg; (void)usr_data;
    if (s_{{prefix_lc}}_fr_cue) esp_timer_stop(s_{{prefix_lc}}_fr_cue);
    if (s_{{prefix_lc}}_fr_held && !s_{{prefix_lc}}_fr_fired) {
        ESP_LOGI(TAG, "{{prefix_lc}}: released before the factory reset — cancelled");
        app_factory_reset_hold_cancelled();
    }
    s_{{prefix_lc}}_fr_held = false;
    s_{{prefix_lc}}_fr_fired = false;
}
static void {{prefix_lc}}_factory_reset_cb(void *arg, void *usr_data)
{
    (void)arg; (void)usr_data;
    if (s_{{prefix_lc}}_fr_cue) esp_timer_stop(s_{{prefix_lc}}_fr_cue);
    if (!s_{{prefix_lc}}_fr_held) {
        s_{{prefix_lc}}_fr_held = true;
        app_factory_reset_hold_started();
    }
    s_{{prefix_lc}}_fr_fired = true;
    ESP_LOGW(TAG, "{{prefix_lc}}: held {{cfg.factory_reset_ms}} ms — factory reset");
    if (app_utils_factory_reset() != ESP_OK) {
        /* The erase failed and the device carries on: end the feedback. */
        app_factory_reset_hold_cancelled();
    }
}
{{/if}}
