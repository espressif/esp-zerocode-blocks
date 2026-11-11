{
    /* A port whose driver is already installed belongs to someone else — on
       port 0 usually the console. Reconfiguring its pins or installing again
       would take the console's UART away (or abort on ESP_ERR_INVALID_STATE),
       so its configuration stands and this block uses it as it is. */
    if (uart_is_driver_installed((uart_port_t){{prefix}}_UART_PORT)) {
        ESP_LOGW(TAG, "UART {{prefix_lc}}: port %d already in use — using it as configured there", {{prefix}}_UART_PORT);
    } else {
        uart_config_t {{prefix_lc}}_uart_cfg = {};
        {{prefix_lc}}_uart_cfg.baud_rate = {{prefix}}_UART_BAUD;
        {{prefix_lc}}_uart_cfg.data_bits = UART_DATA_8_BITS;
        {{prefix_lc}}_uart_cfg.parity = UART_PARITY_DISABLE;
        {{prefix_lc}}_uart_cfg.stop_bits = UART_STOP_BITS_1;
        {{prefix_lc}}_uart_cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
        {{prefix_lc}}_uart_cfg.source_clk = UART_SCLK_DEFAULT;
        /* Degraded, not fatal: a failure leaves the port unusable and says
           why, instead of aborting into a boot loop. */
        esp_err_t {{prefix_lc}}_err = uart_param_config((uart_port_t){{prefix}}_UART_PORT, &{{prefix_lc}}_uart_cfg);
        if ({{prefix_lc}}_err == ESP_OK) {
            {{prefix_lc}}_err = uart_set_pin((uart_port_t){{prefix}}_UART_PORT, {{prefix}}_UART_TX_GPIO, {{prefix}}_UART_RX_GPIO, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
        }
        if ({{prefix_lc}}_err == ESP_OK) {
            {{prefix_lc}}_err = uart_driver_install((uart_port_t){{prefix}}_UART_PORT, {{cfg.rx_buf_size}}, {{cfg.tx_buf_size}}, 0, NULL, 0);
        }
        if ({{prefix_lc}}_err != ESP_OK) {
            ESP_LOGE(TAG, "UART {{prefix_lc}}: port %d not started (%s)", {{prefix}}_UART_PORT, esp_err_to_name({{prefix_lc}}_err));
        } else {
            ESP_LOGI(TAG, "UART {{prefix_lc}}: port=%d tx=%d rx=%d @%d baud", {{prefix}}_UART_PORT, {{prefix}}_UART_TX_GPIO, {{prefix}}_UART_RX_GPIO, {{prefix}}_UART_BAUD);
        }
    }
}
