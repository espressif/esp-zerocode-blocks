{
    /* esp_board_manager_init() ran in app_main BEFORE app_driver_init(), so the
       codec device '{{cfg.device}}' — its I2S data path, its I2C control and its
       amplifier — already exists. This asks for it; it creates nothing. */
    dev_audio_codec_handles_t *{{prefix_lc}}_h = NULL;
    if (esp_board_manager_get_device_handle({{prefix}}_ACODEC_DEVICE, (void **)&{{prefix_lc}}_h) != ESP_OK ||
        {{prefix_lc}}_h == NULL || {{prefix_lc}}_h->codec_dev == NULL) {
        /* Generation refuses a device the board does not declare, so reaching
           here means the board manager failed to bring it up at boot. Degraded,
           not fatal: the product keeps running, it just cannot make a sound. */
        ESP_LOGE(TAG, "{{prefix_lc}}: board audio codec '{{cfg.device}}' has no handle — audio disabled");
    } else {
        int {{prefix_lc}}_board_rate = 0, {{prefix_lc}}_mclk = 0;
        {{prefix_lc}}_board_clock(&{{prefix_lc}}_board_rate, &{{prefix_lc}}_mclk);
        s_{{prefix_lc}}_rate = {{prefix}}_ACODEC_RATE > 0 ? {{prefix}}_ACODEC_RATE
                             : ({{prefix_lc}}_board_rate > 0 ? {{prefix_lc}}_board_rate : 16000);

        esp_codec_dev_sample_info_t {{prefix_lc}}_fs = {};
        {{prefix_lc}}_fs.bits_per_sample = 16;
        {{prefix_lc}}_fs.channel = {{prefix}}_ACODEC_CHANNELS;
        {{prefix_lc}}_fs.sample_rate = (uint32_t)s_{{prefix_lc}}_rate;
        if ({{prefix_lc}}_mclk > 0) {
            {{prefix_lc}}_fs.mclk_multiple = (uint16_t){{prefix_lc}}_mclk;
        }
        if (esp_codec_dev_open({{prefix_lc}}_h->codec_dev, &{{prefix_lc}}_fs) != ESP_CODEC_DEV_OK) {
            ESP_LOGE(TAG, "{{prefix_lc}}: codec '{{cfg.device}}' would not open @%d Hz — audio disabled", s_{{prefix_lc}}_rate);
        } else {
            esp_codec_dev_set_out_vol({{prefix_lc}}_h->codec_dev, {{prefix}}_ACODEC_CODEC_VOL);
            s_{{prefix_lc}}_codec = {{prefix_lc}}_h->codec_dev;
            xTaskCreate({{prefix_lc}}_acodec_task, "{{prefix_lc}}_acodec", 3072, NULL, 5, NULL);
            ESP_LOGI(TAG, "{{prefix_lc}}: board audio codec '{{cfg.device}}' @%d Hz, %d ch, mclk x%d",
                     s_{{prefix_lc}}_rate, {{prefix}}_ACODEC_CHANNELS, {{prefix_lc}}_mclk);
        }
    }
}
