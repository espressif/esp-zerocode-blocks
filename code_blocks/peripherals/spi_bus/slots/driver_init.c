{
    /* Field by field, not a designated initializer: this lands in
     * app_driver.cpp (C++), and spi_bus_config_t's pins sit in anonymous
     * unions (mosi/data0, miso/data1, …) that C++ designators handle poorly. */
    spi_bus_config_t {{prefix_lc}}_bus_cfg = {};
    {{prefix_lc}}_bus_cfg.mosi_io_num = {{prefix}}_SPI_MOSI_GPIO;
    {{prefix_lc}}_bus_cfg.miso_io_num = {{prefix}}_SPI_MISO_GPIO;
    {{prefix_lc}}_bus_cfg.sclk_io_num = {{prefix}}_SPI_SCLK_GPIO;
    {{prefix_lc}}_bus_cfg.quadwp_io_num = -1;
    {{prefix_lc}}_bus_cfg.quadhd_io_num = -1;
    {{prefix_lc}}_bus_cfg.max_transfer_sz = {{prefix}}_SPI_MAX_TRANSFER;
    esp_err_t {{prefix_lc}}_err = spi_bus_initialize((spi_host_device_t){{prefix}}_SPI_HOST, &{{prefix_lc}}_bus_cfg, SPI_DMA_CH_AUTO);
    if ({{prefix_lc}}_err == ESP_ERR_INVALID_STATE) {
        /* Another block (an SPI panel, the board) already brought this host
         * up. Its configuration stands; devices attach to the host either
         * way. Aborting here was a boot loop for any product pairing this
         * block with a panel on the same host — the double-init i2c_bus's
         * adopt-first rule exists to avoid. */
        ESP_LOGW(TAG, "SPI bus {{prefix_lc}}: host %d already initialized elsewhere — using it as configured there", {{prefix}}_SPI_HOST);
    } else {
        ESP_ERROR_CHECK({{prefix_lc}}_err);
        ESP_LOGI(TAG, "SPI bus {{prefix_lc}}: host=%d MOSI=%d MISO=%d SCLK=%d", {{prefix}}_SPI_HOST, {{prefix}}_SPI_MOSI_GPIO, {{prefix}}_SPI_MISO_GPIO, {{prefix}}_SPI_SCLK_GPIO);
    }
}
