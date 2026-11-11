{
    esp_err_t {{prefix_lc}}_err = ESP_OK;
#if defined({{prefix}}_RFID_IF_spi)
    rc522_spi_config_t {{prefix_lc}}_cfg = {};
    {{prefix_lc}}_cfg.host_id = (spi_host_device_t){{prefix}}_RFID_SPI_HOST;
    {{prefix_lc}}_cfg.bus_config = NULL;   /* peripherals/spi_bus initialised it */
    {{prefix_lc}}_cfg.dev_config.spics_io_num = {{prefix}}_RFID_SPI_CS;
    {{prefix_lc}}_cfg.dev_config.clock_speed_hz = 5000000;
    {{prefix_lc}}_cfg.dev_config.queue_size = 1;
    {{prefix_lc}}_cfg.rst_io_num = (gpio_num_t){{prefix}}_RFID_RST;
    {{prefix_lc}}_err = rc522_spi_create(&{{prefix_lc}}_cfg, &s_{{prefix_lc}}_rfid_drv);
#else
    i2c_master_bus_handle_t {{prefix_lc}}_bus = NULL;
    {{prefix_lc}}_err = i2c_master_get_bus_handle((i2c_port_num_t){{prefix}}_RFID_I2C_PORT, &{{prefix_lc}}_bus);
    if ({{prefix_lc}}_err != ESP_OK) {
        ESP_LOGE(TAG, "{{prefix_lc}}: no I2C bus on port %d (%s) - needs a bus provider earlier in the product, or a board that owns the port",
                 (int){{prefix}}_RFID_I2C_PORT, esp_err_to_name({{prefix_lc}}_err));
    } else {
        rc522_i2c_config_t {{prefix_lc}}_cfg = {};
        {{prefix_lc}}_cfg.bus = {{prefix_lc}}_bus;
        {{prefix_lc}}_cfg.dev_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        {{prefix_lc}}_cfg.dev_config.device_address = {{prefix}}_RFID_I2C_ADDR;
        {{prefix_lc}}_cfg.dev_config.scl_speed_hz = 100000;
        {{prefix_lc}}_cfg.rst_io_num = (gpio_num_t){{prefix}}_RFID_RST;
        {{prefix_lc}}_err = rc522_i2c_create(&{{prefix_lc}}_cfg, &s_{{prefix_lc}}_rfid_drv);
    }
#endif
    if ({{prefix_lc}}_err == ESP_OK) {{prefix_lc}}_err = rc522_driver_install(s_{{prefix_lc}}_rfid_drv);
    if ({{prefix_lc}}_err == ESP_OK) {
        rc522_config_t {{prefix_lc}}_sc = {};
        {{prefix_lc}}_sc.driver = s_{{prefix_lc}}_rfid_drv;
        {{prefix_lc}}_sc.poll_interval_ms = {{prefix}}_RFID_POLL_MS;
        {{prefix_lc}}_err = rc522_create(&{{prefix_lc}}_sc, &s_{{prefix_lc}}_rfid);
    }
    if ({{prefix_lc}}_err == ESP_OK) {
        {{prefix_lc}}_err = rc522_register_events(s_{{prefix_lc}}_rfid, RC522_EVENT_PICC_STATE_CHANGED, {{prefix_lc}}_rfid_event, NULL);
    }
    if ({{prefix_lc}}_err == ESP_OK) {{prefix_lc}}_err = rc522_start(s_{{prefix_lc}}_rfid);
    if ({{prefix_lc}}_err != ESP_OK) {
        ESP_LOGE(TAG, "{{prefix_lc}}: card reader not started (%s) - no cards will be read", esp_err_to_name({{prefix_lc}}_err));
    } else {
        ESP_LOGI(TAG, "{{prefix_lc}}: card reader started (spi)");
    }
}
