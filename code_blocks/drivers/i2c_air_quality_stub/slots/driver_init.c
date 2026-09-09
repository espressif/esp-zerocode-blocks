{
    /* ADOPT, THEN CREATE. i2c_new_master_bus() fails on a port that already
       carries a bus — a board that owns the port, or another block that got
       there first. Looking the port up before creating makes "two blocks want
       this bus" benign whichever one runs first, instead of a hard failure at
       boot. See CLAUDE.md → How an I2C block gets its bus. */
    i2c_master_bus_handle_t {{prefix_lc}}_i2c_bus = NULL;
    if (i2c_master_get_bus_handle((i2c_port_num_t){{prefix}}_I2C_PORT, &{{prefix_lc}}_i2c_bus) == ESP_OK) {
        ESP_LOGI(TAG, "Air quality {{prefix_lc}}: I2C port %d already configured — adopted", {{prefix}}_I2C_PORT);
    } else {
        /* Field-by-field rather than a designated-initializer list: this file
           compiles as C++, where the initializer ORDER is fixed and the union
           at the head of i2c_master_bus_config_t makes `.clk_source` awkward
           to name positionally. */
        i2c_master_bus_config_t {{prefix_lc}}_i2c_conf = {};
        {{prefix_lc}}_i2c_conf.i2c_port = {{prefix}}_I2C_PORT;
        /* ESP-IDF 6 types the pin fields as gpio_num_t, and an integer macro
           no longer converts implicitly in C++. SPI's are still plain int,
           which is why spi_bus needs no cast and this does. */
        {{prefix_lc}}_i2c_conf.sda_io_num = (gpio_num_t){{prefix}}_I2C_SDA_GPIO;
        {{prefix_lc}}_i2c_conf.scl_io_num = (gpio_num_t){{prefix}}_I2C_SCL_GPIO;
        {{prefix_lc}}_i2c_conf.clk_source = I2C_CLK_SRC_DEFAULT;
        {{prefix_lc}}_i2c_conf.glitch_ignore_cnt = 7;
        {{prefix_lc}}_i2c_conf.flags.enable_internal_pullup = true;
        esp_err_t {{prefix_lc}}_i2c_err = i2c_new_master_bus(&{{prefix_lc}}_i2c_conf, &{{prefix_lc}}_i2c_bus);
        if ({{prefix_lc}}_i2c_err != ESP_OK) {
            ESP_LOGE(TAG, "Air quality {{prefix_lc}}: i2c_new_master_bus failed: %s", esp_err_to_name({{prefix_lc}}_i2c_err));
        } else {
            /* freq_hz is advisory from here on: the new driver puts SCL speed
               on each DEVICE (i2c_device_config_t.scl_speed_hz), so a sensor
               block's own i2c_freq_hz is what actually clocks its transfers.
               Logged so a mismatch between the two is visible in a device log. */
            ESP_LOGI(TAG, "Air quality {{prefix_lc}}: I2C port %d SDA=%d SCL=%d (devices clock themselves; product default %uHz)",
                     {{prefix}}_I2C_PORT, {{prefix}}_I2C_SDA_GPIO, {{prefix}}_I2C_SCL_GPIO,
                     (unsigned){{prefix}}_I2C_FREQ_HZ);
        }
    }
}
