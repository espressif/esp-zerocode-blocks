static volatile bool s_{{prefix_lc}}_clicking = false;

/* Runs in its own short-lived task: the identify callback is on the CHIP
 * thread, and the click loop blocks for click_count * 2 * click_ms. */
static void {{prefix_lc}}_identify_click_task(void *arg)
{
    app_driver_param_val_t saved;
    app_driver_get_param({{cfg.target_param}}, &saved);
    for (int i = 0; i < {{prefix}}_CLICK_COUNT; ++i) {
        app_driver_param_val_t on  = { .b = true };
        app_driver_param_val_t off = { .b = false };
        app_driver_set_param({{cfg.target_param}}, on,  APP_DRIVER_SOURCE_LOCAL);
        vTaskDelay(pdMS_TO_TICKS({{prefix}}_CLICK_MS));
        app_driver_set_param({{cfg.target_param}}, off, APP_DRIVER_SOURCE_LOCAL);
        vTaskDelay(pdMS_TO_TICKS({{prefix}}_CLICK_MS));
    }
    /* Restore previous state */
    app_driver_set_param({{cfg.target_param}}, saved, APP_DRIVER_SOURCE_LOCAL);
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
