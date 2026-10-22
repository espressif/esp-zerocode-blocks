{
    /* Its own task + an echo-edge ISR, not an esp_timer callback: see block.yml. */
    gpio_set_intr_type((gpio_num_t){{prefix}}_HC_ECHO_GPIO, GPIO_INTR_ANYEDGE);
    esp_err_t {{prefix_lc}}_isr_err = gpio_install_isr_service(0);
    if ({{prefix_lc}}_isr_err == ESP_ERR_INVALID_STATE) {{prefix_lc}}_isr_err = ESP_OK;  /* already installed */
    if ({{prefix_lc}}_isr_err == ESP_OK) {
        {{prefix_lc}}_isr_err = gpio_isr_handler_add((gpio_num_t){{prefix}}_HC_ECHO_GPIO, {{prefix_lc}}_hc_echo_isr, NULL);
    }
    if ({{prefix_lc}}_isr_err != ESP_OK) {
        /* The task still runs and reports every poll as "no echo" — visible,
           not silent. */
        ESP_LOGE(TAG, "HC-SR04 {{prefix_lc}}: echo interrupt not attached (%s)", esp_err_to_name({{prefix_lc}}_isr_err));
    }
    /* Last, so the first measurement already has the echo ISR attached. */
    xTaskCreate({{prefix_lc}}_hc_task_fn, "{{prefix_lc}}_hc", 2560, NULL, 3, &s_{{prefix_lc}}_hc_task);
}
