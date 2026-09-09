{
    /* QSPI bus: four data lines, no D/C pin (commands ride the protocol).
     * Field-by-field instead of CO5300_PANEL_BUS_QSPI_CONFIG(): the macro's
     * designated-initializer order is C-legal but rejected by C++ (this file
     * compiles as C++). Values mirror the macro. */
    spi_bus_config_t {{prefix_lc}}_bus = {};
    {{prefix_lc}}_bus.sclk_io_num = {{prefix}}_LCD_SCLK;
    {{prefix_lc}}_bus.data0_io_num = {{prefix}}_LCD_D0;
    {{prefix_lc}}_bus.data1_io_num = {{prefix}}_LCD_D1;
    {{prefix_lc}}_bus.data2_io_num = {{prefix}}_LCD_D2;
    {{prefix_lc}}_bus.data3_io_num = {{prefix}}_LCD_D3;
    {{prefix_lc}}_bus.max_transfer_sz = {{prefix}}_LCD_HRES * 40 * sizeof(uint16_t);
    ESP_ERROR_CHECK(spi_bus_initialize((spi_host_device_t){{prefix}}_LCD_SPI_HOST,
                                       &{{prefix_lc}}_bus, SPI_DMA_CH_AUTO));

    /* Panel IO in quad mode: 32-bit command phase, 8-bit params, no D/C
     * (mirrors CO5300_PANEL_IO_QSPI_CONFIG). */
    esp_lcd_panel_io_handle_t {{prefix_lc}}_io = NULL;
    esp_lcd_panel_io_spi_config_t {{prefix_lc}}_io_cfg = {};
    /* gpio_num_t casts: IDF 6.x typed these fields (plain int before). */
    {{prefix_lc}}_io_cfg.cs_gpio_num = (gpio_num_t){{prefix}}_LCD_CS;
    {{prefix_lc}}_io_cfg.dc_gpio_num = GPIO_NUM_NC;
    {{prefix_lc}}_io_cfg.pclk_hz = {{prefix}}_LCD_PCLK_HZ;
    {{prefix_lc}}_io_cfg.lcd_cmd_bits = 32;
    {{prefix_lc}}_io_cfg.lcd_param_bits = 8;
    {{prefix_lc}}_io_cfg.spi_mode = 0;
    {{prefix_lc}}_io_cfg.trans_queue_depth = 10;
    {{prefix_lc}}_io_cfg.flags.quad_mode = 1;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t){{prefix}}_LCD_SPI_HOST,
                                             &{{prefix_lc}}_io_cfg, &{{prefix_lc}}_io));

    co5300_vendor_config_t {{prefix_lc}}_vendor = {};
    {{prefix_lc}}_vendor.flags.use_qspi_interface = 1;

    esp_lcd_panel_handle_t {{prefix_lc}}_panel = NULL;
    esp_lcd_panel_dev_config_t {{prefix_lc}}_panel_cfg = {};
    {{prefix_lc}}_panel_cfg.reset_gpio_num = (gpio_num_t){{prefix}}_LCD_RST;
    {{prefix_lc}}_panel_cfg.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
    {{prefix_lc}}_panel_cfg.bits_per_pixel = 16;
    {{prefix_lc}}_panel_cfg.vendor_config = &{{prefix_lc}}_vendor;
    ESP_ERROR_CHECK(esp_lcd_new_panel_co5300({{prefix_lc}}_io, &{{prefix_lc}}_panel_cfg,
                                             &{{prefix_lc}}_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset({{prefix_lc}}_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init({{prefix_lc}}_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off({{prefix_lc}}_panel, true));

    lvgl_port_display_cfg_t {{prefix_lc}}_disp_cfg = {};
    {{prefix_lc}}_disp_cfg.io_handle = {{prefix_lc}}_io;
    {{prefix_lc}}_disp_cfg.panel_handle = {{prefix_lc}}_panel;
    {{prefix_lc}}_disp_cfg.buffer_size = {{prefix}}_LCD_HRES * 40;
    {{prefix_lc}}_disp_cfg.double_buffer = true;
    {{prefix_lc}}_disp_cfg.hres = {{prefix}}_LCD_HRES;
    {{prefix_lc}}_disp_cfg.vres = {{prefix}}_LCD_VRES;
    {{prefix_lc}}_disp_cfg.color_format = LV_COLOR_FORMAT_RGB565;
    {{prefix_lc}}_disp_cfg.flags.buff_dma = 1;
    {{prefix_lc}}_disp_cfg.flags.swap_bytes = 1;
    s_disp = lvgl_port_add_disp(&{{prefix_lc}}_disp_cfg);
    ESP_LOGI(TAG, "CO5300 {{prefix_lc}}: %dx%d on QSPI host %d",
             {{prefix}}_LCD_HRES, {{prefix}}_LCD_VRES, {{prefix}}_LCD_SPI_HOST);
}
