{
    /* Nothing runs until the device is on the bus. Driving a panel through a
       handle that was never created would log a transfer error per byte of
       the init sequence; one line at boot is the report a person can act on. */
    if ({{prefix_lc}}_i2c_attach() != ESP_OK) {
        ESP_LOGE(TAG, "SSD1306 {{prefix_lc}}: not started — no I2C bus");
    } else {
        const uint8_t init[] = {
            0xAE, 0xD5,0x80, 0xA8,0x3F, 0xD3,0x00, 0x40,
            0x8D,0x14, 0x20,0x00, 0xA1, 0xC8,
            0xDA,0x12, 0x81,0xCF, 0xD9,0xF1, 0xDB,0x40,
            0xA4, 0xA6, 0xAF,
        };
        for (size_t i = 0; i < sizeof(init); ++i) {{prefix_lc}}_oled_cmd(init[i]);
        memset(s_{{prefix_lc}}_oled_fb, 0, sizeof(s_{{prefix_lc}}_oled_fb));
        {{prefix_lc}}_oled_flush();
        ESP_LOGI(TAG, "SSD1306 OLED {{prefix_lc}}: 128x64 ready", "");
    }
}
