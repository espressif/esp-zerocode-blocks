{
    s_{{prefix_lc}}_bus = {{cfg.bus_handle_fn}}();
    if (!s_{{prefix_lc}}_bus) {
        ESP_LOGE(TAG, "{{prefix_lc}}: 1-Wire bus handle is NULL — peripheral not initialized?");
    } else {
        /* Degraded, not fatal — the contract every driver block here keeps:
           with no probe the product runs, this probe just does not report
           until a later search finds it. */
        {{prefix_lc}}_ds_bind();
    }
    /* Its own task, NOT an esp_timer callback: a conversion takes 800 ms at
       12-bit and espressif/ds18b20 waits it out inside
       ds18b20_trigger_temperature_conversion(). On the shared esp_timer task
       that stalled every other timer in the product (button scanning, LED
       blinks, other sensors) for the whole conversion, every poll. */
    xTaskCreate({{prefix_lc}}_ds_task, "{{prefix_lc}}_ds", 3072, NULL, 3, NULL);
}
