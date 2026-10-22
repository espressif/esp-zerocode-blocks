{
    /* do/while(0): every failure below breaks out with the display intact. */
    do {
        /* esp_lcd_touch drives the chip through an I2C panel-io handle. */
        i2c_master_bus_handle_t {{prefix_lc}}_i2c = NULL;
        /* ADOPT, THEN CREATE. This runs in the display task, not driver_init, so
           it is not ordered against a product's own peripherals/i2c_bus on the
           same port — and i2c_new_master_bus() fails outright on a port that
           already carries a bus. Looking it up first makes either order work,
           which is what removed mosaico-touch-hub's double-configuration of I2C0.
           See CLAUDE.md → How an I2C block gets its bus. */
        if (i2c_master_get_bus_handle((i2c_port_num_t){{prefix}}_TOUCH_I2C_PORT, &{{prefix_lc}}_i2c) != ESP_OK) {
            i2c_master_bus_config_t {{prefix_lc}}_i2c_cfg = {};
            {{prefix_lc}}_i2c_cfg.i2c_port = {{prefix}}_TOUCH_I2C_PORT;
            {{prefix_lc}}_i2c_cfg.sda_io_num = (gpio_num_t){{prefix}}_TOUCH_SDA;
            {{prefix_lc}}_i2c_cfg.scl_io_num = (gpio_num_t){{prefix}}_TOUCH_SCL;
            {{prefix_lc}}_i2c_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
            {{prefix_lc}}_i2c_cfg.glitch_ignore_cnt = 7;
            {{prefix_lc}}_i2c_cfg.flags.enable_internal_pullup = true;
            esp_err_t {{prefix_lc}}_bus_err = i2c_new_master_bus(&{{prefix_lc}}_i2c_cfg, &{{prefix_lc}}_i2c);
            if ({{prefix_lc}}_bus_err != ESP_OK) {
                ESP_LOGE(TAG, "{{prefix_lc}}: touch I2C bus failed (%s) — display runs without touch", esp_err_to_name({{prefix_lc}}_bus_err));
                break;
            }
        }

        /* Field-by-field instead of ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG(): the
         * macro's designated-initializer ORDER is C-legal but rejected by C++
         * (this file compiles as C++). Values mirror the macro. */
        esp_lcd_panel_io_handle_t {{prefix_lc}}_tio = NULL;
        esp_lcd_panel_io_i2c_config_t {{prefix_lc}}_tio_cfg = {};
        {{prefix_lc}}_tio_cfg.dev_addr = ESP_LCD_TOUCH_IO_I2C_FT5x06_ADDRESS;
        {{prefix_lc}}_tio_cfg.scl_speed_hz = 100000;
        {{prefix_lc}}_tio_cfg.control_phase_bytes = 1;
        {{prefix_lc}}_tio_cfg.dc_bit_offset = 0;
        {{prefix_lc}}_tio_cfg.lcd_cmd_bits = 8;
        {{prefix_lc}}_tio_cfg.flags.disable_control_phase = 1;
        esp_err_t {{prefix_lc}}_tio_err = esp_lcd_new_panel_io_i2c({{prefix_lc}}_i2c, &{{prefix_lc}}_tio_cfg, &{{prefix_lc}}_tio);
        if ({{prefix_lc}}_tio_err != ESP_OK) {
            ESP_LOGE(TAG, "{{prefix_lc}}: touch panel-io failed (%s) — display runs without touch", esp_err_to_name({{prefix_lc}}_tio_err));
            break;
        }

        esp_lcd_touch_config_t {{prefix_lc}}_tp_cfg = {};
        {{prefix_lc}}_tp_cfg.x_max = {{prefix}}_TOUCH_XMAX;
        {{prefix_lc}}_tp_cfg.y_max = {{prefix}}_TOUCH_YMAX;
        {{prefix_lc}}_tp_cfg.rst_gpio_num = (gpio_num_t){{prefix}}_TOUCH_RST;
        {{prefix_lc}}_tp_cfg.int_gpio_num = (gpio_num_t){{prefix}}_TOUCH_INT;
        esp_lcd_touch_handle_t {{prefix_lc}}_tp = NULL;
        /* The driver talks to the chip at create, so an absent controller (a
           non-touch module, an unplugged FPC, a wrong address) is an error HERE.
           Degraded, not fatal: under ESP_ERROR_CHECK that aborted the display task
           and boot-looped the device over a missing touch layer. */
        esp_err_t {{prefix_lc}}_tp_err = esp_lcd_touch_new_i2c_ft5x06({{prefix_lc}}_tio, &{{prefix_lc}}_tp_cfg, &{{prefix_lc}}_tp);
        if ({{prefix_lc}}_tp_err != ESP_OK) {
            ESP_LOGE(TAG, "{{prefix_lc}}: no FT5x06 touch controller answered (%s) — display runs without touch", esp_err_to_name({{prefix_lc}}_tp_err));
            esp_lcd_panel_io_del({{prefix_lc}}_tio);
            break;
        }

        lvgl_port_touch_cfg_t {{prefix_lc}}_touch_cfg = {};
        {{prefix_lc}}_touch_cfg.disp = s_disp;
        {{prefix_lc}}_touch_cfg.handle = {{prefix_lc}}_tp;
        lv_indev_t *{{prefix_lc}}_indev = lvgl_port_add_touch(&{{prefix_lc}}_touch_cfg);
        if ({{prefix_lc}}_indev == NULL) {
            ESP_LOGE(TAG, "{{prefix_lc}}: touch registration failed");
        } else {
            ESP_LOGI(TAG, "Touch {{prefix_lc}}: FT5x06 on I2C%d", {{prefix}}_TOUCH_I2C_PORT);
        }
    } while (0);
}
