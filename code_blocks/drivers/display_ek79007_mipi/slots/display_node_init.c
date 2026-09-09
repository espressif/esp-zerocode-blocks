{
    /* Field-by-field assignment, not designated initializers: a nested
       designator like `.flags.use_dma2d = true` is legal C and rejected by C++,
       and every slot here is compiled as C++ (same rule the SPI panel follows). */

    /* MIPI-DSI has no pins to claim: the D-PHY pads are dedicated, so unlike
       the SPI panels this block spends none of the chip's GPIO budget. */
    esp_lcd_dsi_bus_handle_t {{prefix_lc}}_bus = NULL;
    esp_lcd_dsi_bus_config_t {{prefix_lc}}_bus_cfg = {};
    {{prefix_lc}}_bus_cfg.bus_id = 0;
    {{prefix_lc}}_bus_cfg.num_data_lanes = 2;
    {{prefix_lc}}_bus_cfg.phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT;
    {{prefix_lc}}_bus_cfg.lane_bit_rate_mbps = {{prefix}}_LCD_LANE_MBPS;
    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&{{prefix_lc}}_bus_cfg, &{{prefix_lc}}_bus));

    /* DBI is the command channel (panel init commands); DPI is the pixel stream. */
    esp_lcd_panel_io_handle_t {{prefix_lc}}_io = NULL;
    esp_lcd_dbi_io_config_t {{prefix_lc}}_dbi_cfg = {};
    {{prefix_lc}}_dbi_cfg.virtual_channel = 0;
    {{prefix_lc}}_dbi_cfg.lcd_cmd_bits = 8;
    {{prefix_lc}}_dbi_cfg.lcd_param_bits = 8;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi({{prefix_lc}}_bus, &{{prefix_lc}}_dbi_cfg, &{{prefix_lc}}_io));

    esp_lcd_dpi_panel_config_t {{prefix_lc}}_dpi_cfg = {};
    {{prefix_lc}}_dpi_cfg.dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT;
    {{prefix_lc}}_dpi_cfg.dpi_clock_freq_mhz = {{prefix}}_LCD_DPI_MHZ;
    {{prefix_lc}}_dpi_cfg.virtual_channel = 0;
    /* IDF 6.2 renamed the field and the enum (pixel_format /
     * LCD_COLOR_PIXEL_FORMAT_* -> in_color_format / LCD_COLOR_FMT_*).
     * ESP_IDF_VERSION is the supported detection; the fleet is on master. */
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
    {{prefix_lc}}_dpi_cfg.in_color_format = LCD_COLOR_FMT_RGB565;
#else
    {{prefix_lc}}_dpi_cfg.pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565;
#endif
    {{prefix_lc}}_dpi_cfg.num_fbs = 1;
    {{prefix_lc}}_dpi_cfg.video_timing.h_size = {{prefix}}_LCD_HRES;
    {{prefix_lc}}_dpi_cfg.video_timing.v_size = {{prefix}}_LCD_VRES;
    {{prefix_lc}}_dpi_cfg.video_timing.hsync_back_porch = 160;
    {{prefix_lc}}_dpi_cfg.video_timing.hsync_pulse_width = 20;
    {{prefix_lc}}_dpi_cfg.video_timing.hsync_front_porch = 160;
    {{prefix_lc}}_dpi_cfg.video_timing.vsync_back_porch = 23;
    {{prefix_lc}}_dpi_cfg.video_timing.vsync_pulse_width = 1;
    {{prefix_lc}}_dpi_cfg.video_timing.vsync_front_porch = 12;
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0)
    /* IDF 6 removed flags.use_dma2d; DMA2D is enabled after creation instead
     * (esp_lcd_dpi_panel_enable_dma2d, called below). */
    {{prefix_lc}}_dpi_cfg.flags.use_dma2d = true;
#endif

    ek79007_vendor_config_t {{prefix_lc}}_vendor_cfg = {};
    {{prefix_lc}}_vendor_cfg.mipi_config.dsi_bus = {{prefix_lc}}_bus;
    {{prefix_lc}}_vendor_cfg.mipi_config.dpi_config = &{{prefix_lc}}_dpi_cfg;

    esp_lcd_panel_handle_t {{prefix_lc}}_panel = NULL;
    esp_lcd_panel_dev_config_t {{prefix_lc}}_panel_cfg = {};
    {{prefix_lc}}_panel_cfg.reset_gpio_num = (gpio_num_t){{prefix}}_LCD_RST;
    {{prefix_lc}}_panel_cfg.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
    {{prefix_lc}}_panel_cfg.bits_per_pixel = 16;
    {{prefix_lc}}_panel_cfg.vendor_config = &{{prefix_lc}}_vendor_cfg;
    ESP_ERROR_CHECK(esp_lcd_new_panel_ek79007({{prefix_lc}}_io, &{{prefix_lc}}_panel_cfg, &{{prefix_lc}}_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset({{prefix_lc}}_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init({{prefix_lc}}_panel));
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
    ESP_ERROR_CHECK(esp_lcd_dpi_panel_enable_dma2d({{prefix_lc}}_panel));
#endif

    /* Preprocessor guard, not a runtime if: BACKLIGHT is a compile-time
     * constant, and when it is -1 the shift in pin_bit_mask is a constant
     * negative shift — an error under -Werror=shift-count-negative on IDF 6's
     * toolchain even though the branch is unreachable. */
#if {{prefix}}_LCD_BACKLIGHT >= 0
    {
        gpio_config_t {{prefix_lc}}_bl = {};
        {{prefix_lc}}_bl.pin_bit_mask = 1ULL << {{prefix}}_LCD_BACKLIGHT;
        {{prefix_lc}}_bl.mode = GPIO_MODE_OUTPUT;
        gpio_config(&{{prefix_lc}}_bl);
        gpio_set_level((gpio_num_t){{prefix}}_LCD_BACKLIGHT, 1);
    }
#endif

    /* Buffers in PSRAM, not internal RAM: 1024x600x2 is 1.2 MB a frame against
       768 KB of SRAM. buff_dma stays OFF with spiram buffers — DMA-capable and
       PSRAM-resident are mutually exclusive here. */
    lvgl_port_display_cfg_t {{prefix_lc}}_disp_cfg = {};
    {{prefix_lc}}_disp_cfg.io_handle = {{prefix_lc}}_io;
    {{prefix_lc}}_disp_cfg.panel_handle = {{prefix_lc}}_panel;
    {{prefix_lc}}_disp_cfg.buffer_size = {{prefix}}_LCD_HRES * {{prefix}}_LCD_BUF_LINES;
    {{prefix_lc}}_disp_cfg.double_buffer = true;
    {{prefix_lc}}_disp_cfg.hres = {{prefix}}_LCD_HRES;
    {{prefix_lc}}_disp_cfg.vres = {{prefix}}_LCD_VRES;
    {{prefix_lc}}_disp_cfg.color_format = LV_COLOR_FORMAT_RGB565;
    {{prefix_lc}}_disp_cfg.flags.buff_dma = 0;
    {{prefix_lc}}_disp_cfg.flags.buff_spiram = 1;

    lvgl_port_display_dsi_cfg_t {{prefix_lc}}_dsi_cfg = {};
    {{prefix_lc}}_dsi_cfg.flags.avoid_tearing = false;
    s_disp = lvgl_port_add_disp_dsi(&{{prefix_lc}}_disp_cfg, &{{prefix_lc}}_dsi_cfg);
    ESP_LOGI(TAG, "EK79007 {{prefix_lc}}: %dx%d over MIPI-DSI (%d MHz DPI)",
             {{prefix}}_LCD_HRES, {{prefix}}_LCD_VRES, {{prefix}}_LCD_DPI_MHZ);
}
