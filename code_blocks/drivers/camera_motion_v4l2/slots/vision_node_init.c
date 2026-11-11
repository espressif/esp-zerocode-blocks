{
    /* esp_video owns the whole pipeline: it brings up the SCCB bus, probes the
       sensor through esp_cam_sensor, configures IDF's DVP controller and then
       publishes the result as a V4L2 device. That is why there is no sensor
       model named anywhere here — the probe decides. */
    esp_video_init_dvp_config_t {{prefix_lc}}_dvp = {};
    /* ADOPT, THEN CREATE — the I2C blocks' rule. If something already owns
       the SCCB port (peripherals/i2c_bus, a board, a display's touch), hand
       esp_video that bus: asking it to create one fails on an occupied port,
       and the camera came up dead in any product with I2C on port 0. The
       sensor then sits on that bus's pins, so pin_sda/pin_scl go unused. */
    i2c_master_bus_handle_t {{prefix_lc}}_sccb_bus = NULL;
    if (i2c_master_get_bus_handle((i2c_port_num_t){{prefix}}_CAM_SCCB_PORT, &{{prefix_lc}}_sccb_bus) == ESP_OK) {
        {{prefix_lc}}_dvp.sccb_config.init_sccb = false;
        {{prefix_lc}}_dvp.sccb_config.i2c_handle = {{prefix_lc}}_sccb_bus;
        ESP_LOGI(TAG, "{{prefix_lc}}: SCCB on the existing I2C%d bus", {{prefix}}_CAM_SCCB_PORT);
    } else {
        {{prefix_lc}}_dvp.sccb_config.init_sccb = true;
        {{prefix_lc}}_dvp.sccb_config.i2c_config.port = {{prefix}}_CAM_SCCB_PORT;
        {{prefix_lc}}_dvp.sccb_config.i2c_config.scl_pin = (gpio_num_t){{prefix}}_CAM_SCL;
        {{prefix_lc}}_dvp.sccb_config.i2c_config.sda_pin = (gpio_num_t){{prefix}}_CAM_SDA;
    }
    {{prefix_lc}}_dvp.sccb_config.freq = {{prefix}}_CAM_SCCB_HZ;
    {{prefix_lc}}_dvp.reset_pin = (gpio_num_t){{prefix}}_CAM_RESET;
    {{prefix_lc}}_dvp.pwdn_pin = (gpio_num_t){{prefix}}_CAM_PWDN;
    {{prefix_lc}}_dvp.xclk_freq = {{prefix}}_CAM_XCLK_HZ;

    {{prefix_lc}}_dvp.dvp_pin.data_width = CAM_CTLR_DATA_WIDTH_8;
    {{prefix_lc}}_dvp.dvp_pin.data_io[0] = (gpio_num_t){{prefix}}_CAM_D0;
    {{prefix_lc}}_dvp.dvp_pin.data_io[1] = (gpio_num_t){{prefix}}_CAM_D1;
    {{prefix_lc}}_dvp.dvp_pin.data_io[2] = (gpio_num_t){{prefix}}_CAM_D2;
    {{prefix_lc}}_dvp.dvp_pin.data_io[3] = (gpio_num_t){{prefix}}_CAM_D3;
    {{prefix_lc}}_dvp.dvp_pin.data_io[4] = (gpio_num_t){{prefix}}_CAM_D4;
    {{prefix_lc}}_dvp.dvp_pin.data_io[5] = (gpio_num_t){{prefix}}_CAM_D5;
    {{prefix_lc}}_dvp.dvp_pin.data_io[6] = (gpio_num_t){{prefix}}_CAM_D6;
    {{prefix_lc}}_dvp.dvp_pin.data_io[7] = (gpio_num_t){{prefix}}_CAM_D7;
    {{prefix_lc}}_dvp.dvp_pin.vsync_io = (gpio_num_t){{prefix}}_CAM_VSYNC;
    {{prefix_lc}}_dvp.dvp_pin.de_io = (gpio_num_t){{prefix}}_CAM_DE;
    {{prefix_lc}}_dvp.dvp_pin.pclk_io = (gpio_num_t){{prefix}}_CAM_PCLK;
    {{prefix_lc}}_dvp.dvp_pin.xclk_io = (gpio_num_t){{prefix}}_CAM_XCLK;

    esp_video_init_config_t {{prefix_lc}}_vcfg = {};
    {{prefix_lc}}_vcfg.dvp = &{{prefix_lc}}_dvp;

    esp_err_t {{prefix_lc}}_err = esp_video_init(&{{prefix_lc}}_vcfg);
    if ({{prefix_lc}}_err != ESP_OK) {
        /* No camera is a degraded state, not a boot blocker — same contract as
           drivers/camera_motion_dvp. The product still runs; motion never fires. */
        ESP_LOGE(TAG, "{{prefix_lc}}: camera init failed (%s) — motion detection disabled",
                 esp_err_to_name({{prefix_lc}}_err));
    } else if ({{prefix_lc}}_stream_start() != ESP_OK) {
        ESP_LOGE(TAG, "{{prefix_lc}}: capture setup failed — motion detection disabled");
    } else {
        xTaskCreate({{prefix_lc}}_motion_task, "{{prefix_lc}}_motion", 4096, NULL, 5, NULL);
        ESP_LOGI(TAG, "Camera {{prefix_lc}}: motion detection at %dx%d, poll %d ms",
                 {{prefix}}_CAM_W, {{prefix}}_CAM_H, {{prefix}}_CAM_POLL_MS);
    }
}
