static QueueHandle_t s_{{prefix_lc}}_intr_q = NULL;

static void IRAM_ATTR {{prefix_lc}}_intr_isr(void *arg)
{
    BaseType_t hp = pdFALSE;
    uint32_t now = xTaskGetTickCountFromISR();
    xQueueSendFromISR(s_{{prefix_lc}}_intr_q, &now, &hp);
    if (hp == pdTRUE) portYIELD_FROM_ISR();
}

static void {{prefix_lc}}_intr_task(void *arg)
{
    uint32_t evt;
    for (;;) {
        if (xQueueReceive(s_{{prefix_lc}}_intr_q, &evt, portMAX_DELAY)) {
            int level = gpio_get_level((gpio_num_t){{prefix}}_INTR_GPIO);
            bool asserted = (level == {{prefix}}_INTR_ACTIVE_LEVEL);
            app_driver_param_val_t pv = { .b = asserted };
            app_driver_set_param({{cfg.target_param}}, pv, APP_DRIVER_SOURCE_LOCAL);
        }
    }
}
