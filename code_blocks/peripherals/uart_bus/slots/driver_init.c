{
    uart_config_t {{prefix_lc}}_uart_cfg = {
        .baud_rate = {{prefix}}_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_param_config((uart_port_t){{prefix}}_UART_PORT, &{{prefix_lc}}_uart_cfg));
    ESP_ERROR_CHECK(uart_set_pin((uart_port_t){{prefix}}_UART_PORT, {{prefix}}_UART_TX_GPIO, {{prefix}}_UART_RX_GPIO, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install((uart_port_t){{prefix}}_UART_PORT, {{cfg.rx_buf_size}}, {{cfg.tx_buf_size}}, 0, NULL, 0));
    ESP_LOGI(TAG, "UART {{prefix_lc}}: port=%d tx=%d rx=%d @%d baud", {{prefix}}_UART_PORT, {{prefix}}_UART_TX_GPIO, {{prefix}}_UART_RX_GPIO, {{prefix}}_UART_BAUD);
}
