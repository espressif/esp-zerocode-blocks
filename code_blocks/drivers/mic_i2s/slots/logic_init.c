{
    /* ESP-IDF 6 deleted the i2s_port_t enum; i2s_chan_config_t.id is a plain
       int now, so the cast is both unnecessary and a hard compile error. */
    i2s_chan_config_t {{prefix_lc}}_chan_cfg =
        I2S_CHANNEL_DEFAULT_CONFIG({{prefix}}_MIC_PORT, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&{{prefix_lc}}_chan_cfg, NULL, &s_{{prefix_lc}}_rx));

    i2s_std_config_t {{prefix_lc}}_std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG({{prefix}}_MIC_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = (gpio_num_t){{prefix}}_MIC_BCLK_GPIO,
            .ws   = (gpio_num_t){{prefix}}_MIC_WS_GPIO,
            .dout = I2S_GPIO_UNUSED,
            .din  = (gpio_num_t){{prefix}}_MIC_DIN_GPIO,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    /* MONO defaults to the LEFT slot; follow the breakout's L/R strapping. */
    {{prefix_lc}}_std_cfg.slot_cfg.slot_mask =
        {{prefix}}_MIC_SLOT_RIGHT ? I2S_STD_SLOT_RIGHT : I2S_STD_SLOT_LEFT;

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_{{prefix_lc}}_rx, &{{prefix_lc}}_std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(s_{{prefix_lc}}_rx));

    xTaskCreate({{prefix_lc}}_mic_task, "{{prefix_lc}}_mic", 3072, NULL, 5, NULL);

    ESP_LOGI(TAG, "Mic {{prefix_lc}}: I2S%d BCLK=%d WS=%d DIN=%d @%d Hz, %d-sample window, %s slot",
             {{prefix}}_MIC_PORT, {{prefix}}_MIC_BCLK_GPIO, {{prefix}}_MIC_WS_GPIO,
             {{prefix}}_MIC_DIN_GPIO, {{prefix}}_MIC_SAMPLE_RATE, {{prefix}}_MIC_WINDOW,
             {{prefix}}_MIC_SLOT_RIGHT ? "right" : "left");
}
