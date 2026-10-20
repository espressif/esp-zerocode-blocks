static QueueHandle_t s_{{prefix_lc}}_intr_q = NULL;

static void IRAM_ATTR {{prefix_lc}}_intr_isr(void *arg)
{
    BaseType_t hp = pdFALSE;
    uint32_t now = xTaskGetTickCountFromISR();
    xQueueSendFromISR(s_{{prefix_lc}}_intr_q, &now, &hp);
    if (hp == pdTRUE) portYIELD_FROM_ISR();
}

static void {{prefix_lc}}_intr_publish(void)
{
    int level = gpio_get_level((gpio_num_t){{prefix}}_INTR_GPIO);
    bool asserted = (level == {{prefix}}_INTR_ACTIVE_LEVEL);
    app_driver_param_val_t pv = { .b = asserted };
    app_driver_set_param({{cfg.target_param}}, pv, APP_DRIVER_SOURCE_LOCAL);
}

static void {{prefix_lc}}_intr_task(void *arg)
{
    uint32_t evt;
    for (;;) {
        if (xQueueReceive(s_{{prefix_lc}}_intr_q, &evt, portMAX_DELAY)) {
            /* Debounce by settling: a bouncing contact fires a burst of edges.
               Wait for it to stop, drop the rest of the burst, then read the
               level ONCE — the settled level is the state, however many edges
               it took to get there. set_param dedups an unchanged result. */
            if ({{prefix}}_INTR_DEBOUNCE_MS > 0) {
                TickType_t settle = pdMS_TO_TICKS({{prefix}}_INTR_DEBOUNCE_MS);
                vTaskDelay(settle ? settle : 1);
                while (xQueueReceive(s_{{prefix_lc}}_intr_q, &evt, 0) == pdTRUE) { }
            }
            {{prefix_lc}}_intr_publish();
        }
    }
}
