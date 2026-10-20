{
    /* ESP-IDF 6 deleted the i2s_port_t enum; i2s_chan_config_t.id is a plain
       int now, so the cast is both unnecessary and a hard compile error. */
    i2s_chan_config_t {{prefix_lc}}_chan_cfg =
        I2S_CHANNEL_DEFAULT_CONFIG({{prefix}}_MIC_PORT, I2S_ROLE_MASTER);
    /* Log and give up rather than ESP_ERROR_CHECK: a microphone that cannot
       start (no free I2S direction, a pin the chip lacks) must not take the
       rest of the product down with a boot loop. */
    esp_err_t {{prefix_lc}}_err = i2s_new_channel(&{{prefix_lc}}_chan_cfg, NULL, &s_{{prefix_lc}}_rx);
    if ({{prefix_lc}}_err != ESP_OK) {
        ESP_LOGE(TAG, "Mic {{prefix_lc}}: no I2S RX channel on port %d: %s",
                 {{prefix}}_MIC_PORT, esp_err_to_name({{prefix_lc}}_err));
        s_{{prefix_lc}}_rx = NULL;
        return;
    }

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

    {{prefix_lc}}_err = i2s_channel_init_std_mode(s_{{prefix_lc}}_rx, &{{prefix_lc}}_std_cfg);
    if ({{prefix_lc}}_err == ESP_OK) {{prefix_lc}}_err = i2s_channel_enable(s_{{prefix_lc}}_rx);
    if ({{prefix_lc}}_err != ESP_OK) {
        ESP_LOGE(TAG, "Mic {{prefix_lc}}: I2S RX setup failed: %s", esp_err_to_name({{prefix_lc}}_err));
        i2s_del_channel(s_{{prefix_lc}}_rx);
        s_{{prefix_lc}}_rx = NULL;
        return;
    }

    /* The port actually taken — the configured one may be -1 (auto). */
    i2s_chan_info_t {{prefix_lc}}_info = {};
    i2s_channel_get_info(s_{{prefix_lc}}_rx, &{{prefix_lc}}_info);

    xTaskCreate({{prefix_lc}}_mic_task, "{{prefix_lc}}_mic", 3072, NULL, 5, NULL);

    ESP_LOGI(TAG, "Mic {{prefix_lc}}: I2S%d BCLK=%d WS=%d DIN=%d @%d Hz, %d-sample window, %s slot",
             {{prefix_lc}}_info.id, {{prefix}}_MIC_BCLK_GPIO, {{prefix}}_MIC_WS_GPIO,
             {{prefix}}_MIC_DIN_GPIO, {{prefix}}_MIC_SAMPLE_RATE, {{prefix}}_MIC_WINDOW,
             {{prefix}}_MIC_SLOT_RIGHT ? "right" : "left");
}
