{
    spi_bus_config_t {{prefix_lc}}_bus_cfg = {
        .mosi_io_num = {{prefix}}_SPI_MOSI_GPIO,
        .miso_io_num = {{prefix}}_SPI_MISO_GPIO,
        .sclk_io_num = {{prefix}}_SPI_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = {{prefix}}_SPI_MAX_TRANSFER,
    };
    ESP_ERROR_CHECK(spi_bus_initialize((spi_host_device_t){{prefix}}_SPI_HOST, &{{prefix_lc}}_bus_cfg, SPI_DMA_CH_AUTO));
    ESP_LOGI(TAG, "SPI bus {{prefix_lc}}: host=%d MOSI=%d MISO=%d SCLK=%d", {{prefix}}_SPI_HOST, {{prefix}}_SPI_MOSI_GPIO, {{prefix}}_SPI_MISO_GPIO, {{prefix}}_SPI_SCLK_GPIO);
}
