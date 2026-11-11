static QueueHandle_t s_{{prefix_lc}}_intr_q = NULL;
/* Last published state: -1 before the boot level is read. */
static int s_{{prefix_lc}}_intr_last = -1;

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
    bool asserted = (level == {{prefix}}_INTR_PRESS_LEVEL);
    int prev = s_{{prefix_lc}}_intr_last;
    if (prev == (int)asserted) {
        return; /* a burst that settled where it started */
    }
    s_{{prefix_lc}}_intr_last = asserted;
    app_driver_param_val_t pv = { .b = asserted };
#if {{prefix}}_INTR_MOMENTARY
    /* Momentary: the press is an EVENT (always delivered), the release is the
       state going back to false. The boot level is state, never a press. */
    if (asserted && prev != -1) {
        app_driver_fire_event({{cfg.target_param}}, pv, APP_DRIVER_SOURCE_LOCAL);
        return;
    }
#endif
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
               it took to get there. An unchanged result publishes nothing. */
            if ({{prefix}}_INTR_DEBOUNCE_MS > 0) {
                TickType_t settle = pdMS_TO_TICKS({{prefix}}_INTR_DEBOUNCE_MS);
                vTaskDelay(settle ? settle : 1);
                while (xQueueReceive(s_{{prefix_lc}}_intr_q, &evt, 0) == pdTRUE) { }
            }
            {{prefix_lc}}_intr_publish();
        }
    }
}
