static volatile bool s_{{prefix_lc}}_clicking = false;

/* Runs in its own short-lived task: the identify callback is on the CHIP
 * thread, and the click loop blocks for click_count * 2 * click_ms.
 *
 * The clicks drive the HARDWARE only (app_driver_apply_param), never the bus.
 * A click through set_param was a real state change: every controller
 * (Matter, MQTT) was told On/Off seven times, nvs_persist_bool wrote flash
 * seven times, auto_off_timer re-armed and haptic_on_change buzzed. The bus
 * value never moves, so at the end the hardware is put back to whatever the
 * bus holds THEN — including a real command that arrived mid-identify. */
static void {{prefix_lc}}_identify_click_task(void *arg)
{
    for (int i = 0; i < {{prefix}}_CLICK_COUNT; ++i) {
        app_driver_param_val_t on  = { .b = true };
        app_driver_param_val_t off = { .b = false };
        app_driver_apply_param({{cfg.target_param}}, on);
        vTaskDelay(pdMS_TO_TICKS({{prefix}}_CLICK_MS));
        app_driver_apply_param({{cfg.target_param}}, off);
        vTaskDelay(pdMS_TO_TICKS({{prefix}}_CLICK_MS));
    }
    /* Back to the bus's state. A param never written since boot reads as the
     * zeroed union (false), which is also the relay's born-safe idle level. */
    app_driver_param_val_t now = {};
    app_driver_get_param({{cfg.target_param}}, &now);
    app_driver_apply_param({{cfg.target_param}}, now);
    s_{{prefix_lc}}_clicking = false;
    vTaskDelete(NULL);
}

static void {{prefix_lc}}_identify_click(void)
{
    if (s_{{prefix_lc}}_clicking) {
        return;
    }
    s_{{prefix_lc}}_clicking = true;
    if (xTaskCreate({{prefix_lc}}_identify_click_task, "{{prefix_lc}}_idfy", 2560, NULL,
                    tskIDLE_PRIORITY + 1, NULL) != pdPASS) {
        s_{{prefix_lc}}_clicking = false;
        ESP_LOGE(TAG, "{{prefix_lc}}: failed to start identify click task");
    }
}
