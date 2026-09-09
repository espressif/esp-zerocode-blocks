{
    /* ESP-IDF 6 deleted the i2s_port_t enum; i2s_chan_config_t.id is a plain
       int now, so the cast is both unnecessary and a hard compile error. */
    i2s_chan_config_t {{prefix_lc}}_chan_cfg =
        I2S_CHANNEL_DEFAULT_CONFIG({{prefix}}_VMIC_PORT, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&{{prefix_lc}}_chan_cfg, NULL, &s_{{prefix_lc}}_vmic_rx));

    i2s_std_config_t {{prefix_lc}}_std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG({{prefix}}_VMIC_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = (gpio_num_t){{prefix}}_VMIC_BCLK_GPIO,
            .ws   = (gpio_num_t){{prefix}}_VMIC_WS_GPIO,
            .dout = I2S_GPIO_UNUSED,
            .din  = (gpio_num_t){{prefix}}_VMIC_DIN_GPIO,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    {{prefix_lc}}_std_cfg.slot_cfg.slot_mask =
        {{prefix}}_VMIC_SLOT_RIGHT ? I2S_STD_SLOT_RIGHT : I2S_STD_SLOT_LEFT;

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_{{prefix_lc}}_vmic_rx, &{{prefix_lc}}_std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(s_{{prefix_lc}}_vmic_rx));

    ESP_LOGI(TAG, "Voice mic {{prefix_lc}}: I2S%d BCLK=%d WS=%d DIN=%d @%d Hz, %s slot",
             {{prefix}}_VMIC_PORT, {{prefix}}_VMIC_BCLK_GPIO, {{prefix}}_VMIC_WS_GPIO,
             {{prefix}}_VMIC_DIN_GPIO, {{prefix}}_VMIC_SAMPLE_RATE,
             {{prefix}}_VMIC_SLOT_RIGHT ? "right" : "left");
}
