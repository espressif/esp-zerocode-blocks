{
    /* SPI bus sized for one partial LVGL buffer per transfer. */
    spi_bus_config_t {{prefix_lc}}_bus = {};
    {{prefix_lc}}_bus.sclk_io_num = {{prefix}}_LCD_SCLK;
    {{prefix_lc}}_bus.mosi_io_num = {{prefix}}_LCD_MOSI;
    {{prefix_lc}}_bus.miso_io_num = -1;
    {{prefix_lc}}_bus.quadwp_io_num = -1;
    {{prefix_lc}}_bus.quadhd_io_num = -1;
    {{prefix_lc}}_bus.max_transfer_sz = {{prefix}}_LCD_HRES * 40 * sizeof(uint16_t);
    ESP_ERROR_CHECK(spi_bus_initialize((spi_host_device_t){{prefix}}_LCD_SPI_HOST,
                                       &{{prefix_lc}}_bus, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t {{prefix_lc}}_io = NULL;
    esp_lcd_panel_io_spi_config_t {{prefix_lc}}_io_cfg = {};
    /* ESP-IDF 6 retyped esp_lcd's gpio fields from int to gpio_num_t, so a
       plain integer macro no longer converts implicitly in C++. */
    {{prefix_lc}}_io_cfg.dc_gpio_num = (gpio_num_t){{prefix}}_LCD_DC;
    {{prefix_lc}}_io_cfg.cs_gpio_num = (gpio_num_t){{prefix}}_LCD_CS;
    {{prefix_lc}}_io_cfg.pclk_hz = {{prefix}}_LCD_PCLK_HZ;
    {{prefix_lc}}_io_cfg.lcd_cmd_bits = 8;
    {{prefix_lc}}_io_cfg.lcd_param_bits = 8;
    {{prefix_lc}}_io_cfg.spi_mode = 0;
    {{prefix_lc}}_io_cfg.trans_queue_depth = 10;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t){{prefix}}_LCD_SPI_HOST,
                                             &{{prefix_lc}}_io_cfg, &{{prefix_lc}}_io));

    esp_lcd_panel_handle_t {{prefix_lc}}_panel = NULL;
    esp_lcd_panel_dev_config_t {{prefix_lc}}_panel_cfg = {};
    {{prefix_lc}}_panel_cfg.reset_gpio_num = (gpio_num_t){{prefix}}_LCD_RST;
    {{prefix_lc}}_panel_cfg.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR;
    {{prefix_lc}}_panel_cfg.bits_per_pixel = 16;
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341({{prefix_lc}}_io, &{{prefix_lc}}_panel_cfg,
                                              &{{prefix_lc}}_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset({{prefix_lc}}_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init({{prefix_lc}}_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off({{prefix_lc}}_panel, true));

    if ({{prefix}}_LCD_BACKLIGHT >= 0) {
        gpio_config_t {{prefix_lc}}_bl = {};
        {{prefix_lc}}_bl.pin_bit_mask = 1ULL << {{prefix}}_LCD_BACKLIGHT;
        {{prefix_lc}}_bl.mode = GPIO_MODE_OUTPUT;
        gpio_config(&{{prefix_lc}}_bl);
        gpio_set_level((gpio_num_t){{prefix}}_LCD_BACKLIGHT, 1);
    }

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
    ESP_LOGI(TAG, "ILI9341 {{prefix_lc}}: %dx%d on SPI%d",
             {{prefix}}_LCD_HRES, {{prefix}}_LCD_VRES, {{prefix}}_LCD_SPI_HOST);
}
