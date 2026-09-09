{
    /* 1. Power the codec rail first and let it settle — every register write
          below is over I2C to a chip that must already be alive. */
    {{prefix_lc}}_ctrl_pin({{prefix}}_SPK_CODEC_PW, 1);
    /* 2. Amplifier OFF for now. It is enabled LAST, once the codec is
          configured and I2S is running: switching on a class-D amp while its
          input is undefined is how a board greets its owner with a bang. */
    {{prefix_lc}}_ctrl_pin({{prefix}}_SPK_PA_GPIO, !{{prefix}}_SPK_PA_ACTIVE);
    vTaskDelay(pdMS_TO_TICKS(10));

    /* 3. I2S channel. MCLK is not optional on an ES8311 — it clocks the codec's
          internal converters, and without it the part stays silent with no
          error anywhere. */
    i2s_chan_config_t {{prefix_lc}}_chan_cfg =
        I2S_CHANNEL_DEFAULT_CONFIG({{prefix}}_SPK_PORT, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&{{prefix_lc}}_chan_cfg, &s_{{prefix_lc}}_tx, NULL));

    i2s_std_config_t {{prefix_lc}}_std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG({{prefix}}_SPK_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = (gpio_num_t){{prefix}}_SPK_MCLK_GPIO,
            .bclk = (gpio_num_t){{prefix}}_SPK_BCLK_GPIO,
            .ws   = (gpio_num_t){{prefix}}_SPK_WS_GPIO,
            .dout = (gpio_num_t){{prefix}}_SPK_DOUT_GPIO,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_{{prefix_lc}}_tx, &{{prefix_lc}}_std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(s_{{prefix_lc}}_tx));

    /* 4. Codec control + data interfaces. The I2C port is SHARED with the
          board's other devices (on a Mosaico: touch, IMU, magnetometers, fuel
          gauge), so it is pre-installed elsewhere and only referenced here —
          this block must never install or tear down that bus. */
    audio_codec_i2c_cfg_t {{prefix_lc}}_i2c_cfg = {};
    {{prefix_lc}}_i2c_cfg.port = {{prefix}}_SPK_I2C_PORT;
    {{prefix_lc}}_i2c_cfg.addr = {{prefix}}_SPK_I2C_ADDR;
    const audio_codec_ctrl_if_t *{{prefix_lc}}_ctrl = audio_codec_new_i2c_ctrl(&{{prefix_lc}}_i2c_cfg);

    audio_codec_i2s_cfg_t {{prefix_lc}}_i2s_cfg = {};
    {{prefix_lc}}_i2s_cfg.port = {{prefix}}_SPK_PORT;
    {{prefix_lc}}_i2s_cfg.tx_handle = s_{{prefix_lc}}_tx;
    const audio_codec_data_if_t *{{prefix_lc}}_data = audio_codec_new_i2s_data(&{{prefix_lc}}_i2s_cfg);

    es8311_codec_cfg_t {{prefix_lc}}_es_cfg = {};
    {{prefix_lc}}_es_cfg.ctrl_if = {{prefix_lc}}_ctrl;
    {{prefix_lc}}_es_cfg.gpio_if = audio_codec_new_gpio();
    {{prefix_lc}}_es_cfg.codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC;
    /* The amp pin is driven by THIS block, in the order described above, so the
       codec driver is told there is none. Handing it over would enable the amp
       inside codec_open, before the I2S stream is primed. */
    {{prefix_lc}}_es_cfg.pa_pin = -1;
    {{prefix_lc}}_es_cfg.use_mclk = true;
    {{prefix_lc}}_es_cfg.master_mode = false;   /* the ESP is I2S master; the codec is not */
    const audio_codec_if_t *{{prefix_lc}}_codec_if = es8311_codec_new(&{{prefix_lc}}_es_cfg);

    esp_codec_dev_cfg_t {{prefix_lc}}_dev_cfg = {};
    {{prefix_lc}}_dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_OUT;
    {{prefix_lc}}_dev_cfg.codec_if = {{prefix_lc}}_codec_if;
    {{prefix_lc}}_dev_cfg.data_if = {{prefix_lc}}_data;
    s_{{prefix_lc}}_codec = esp_codec_dev_new(&{{prefix_lc}}_dev_cfg);

    if (!s_{{prefix_lc}}_codec) {
        /* A dead codec is a degraded state, not a boot blocker — the product
           still runs, it just cannot make a sound. Same contract as every other
           driver block here. */
        ESP_LOGE(TAG, "{{prefix_lc}}: ES8311 not found at 0x%02x — audio disabled",
                 {{prefix}}_SPK_I2C_ADDR);
    } else {
        esp_codec_dev_sample_info_t {{prefix_lc}}_fs = {};
        {{prefix_lc}}_fs.bits_per_sample = 16;
        {{prefix_lc}}_fs.channel = 1;
        {{prefix_lc}}_fs.sample_rate = {{prefix}}_SPK_SAMPLE_RATE;
        esp_codec_dev_open(s_{{prefix_lc}}_codec, &{{prefix_lc}}_fs);
        /* Fixed codec level: volume is applied in the digital generator so
           tone/volume/mute behave the same whichever speaker block a product
           picks. */
        esp_codec_dev_set_out_vol(s_{{prefix_lc}}_codec, 80);

        xTaskCreate({{prefix_lc}}_spk_task, "{{prefix_lc}}_spk", 3072, NULL, 5, NULL);
        /* 5. Amplifier last, with silence already flowing. */
        {{prefix_lc}}_ctrl_pin({{prefix}}_SPK_PA_GPIO, {{prefix}}_SPK_PA_ACTIVE);
        ESP_LOGI(TAG, "Speaker {{prefix_lc}}: ES8311 on I2S%d (MCLK=%d BCLK=%d WS=%d DOUT=%d) @%d Hz",
                 {{prefix}}_SPK_PORT, {{prefix}}_SPK_MCLK_GPIO, {{prefix}}_SPK_BCLK_GPIO,
                 {{prefix}}_SPK_WS_GPIO, {{prefix}}_SPK_DOUT_GPIO, {{prefix}}_SPK_SAMPLE_RATE);
    }
}
