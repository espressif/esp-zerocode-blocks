{
    /* ESP-IDF 6 deleted the i2s_port_t enum; i2s_chan_config_t.id is a plain
       int now, so the cast is both unnecessary and a hard compile error. */
    i2s_chan_config_t {{prefix_lc}}_chan_cfg =
        I2S_CHANNEL_DEFAULT_CONFIG({{prefix}}_SPK_PORT, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&{{prefix_lc}}_chan_cfg, &s_{{prefix_lc}}_tx, NULL));

    i2s_std_config_t {{prefix_lc}}_std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG({{prefix}}_SPK_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = (gpio_num_t){{prefix}}_SPK_BCLK_GPIO,
            .ws   = (gpio_num_t){{prefix}}_SPK_WS_GPIO,
            .dout = (gpio_num_t){{prefix}}_SPK_DOUT_GPIO,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_{{prefix_lc}}_tx, &{{prefix_lc}}_std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(s_{{prefix_lc}}_tx));

    /* Priority 5: above idle work, below the framework stacks' own tasks. */
    xTaskCreate({{prefix_lc}}_spk_task, "{{prefix_lc}}_spk", 3072, NULL, 5, NULL);

    ESP_LOGI(TAG, "Speaker {{prefix_lc}}: I2S%d BCLK=%d WS=%d DOUT=%d @%d Hz",
             {{prefix}}_SPK_PORT, {{prefix}}_SPK_BCLK_GPIO, {{prefix}}_SPK_WS_GPIO,
             {{prefix}}_SPK_DOUT_GPIO, {{prefix}}_SPK_SAMPLE_RATE);
}
