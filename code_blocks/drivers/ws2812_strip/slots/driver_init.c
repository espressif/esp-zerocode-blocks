{
    /* Zero-init + assign: newer IDF/components add fields to these structs and
       the build runs with -Werror=missing-field-initializers. */
    led_strip_config_t {{prefix_lc}}_strip_cfg = {};
    {{prefix_lc}}_strip_cfg.strip_gpio_num = {{prefix}}_LED_GPIO;
    {{prefix_lc}}_strip_cfg.max_leds = {{prefix}}_NUM_LEDS;
    {{prefix_lc}}_strip_cfg.led_model = LED_MODEL_WS2812;
    /* led_strip 3.x replaced the `led_pixel_format` enum (LED_PIXEL_FORMAT_GRB)
       with `color_component_format`, a struct describing each component's
       position, width and count — so RGBW and 16-bit-per-component strips can
       be expressed at all. The GRB helper macro is the like-for-like swap. */
    {{prefix_lc}}_strip_cfg.color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB;
    {{prefix_lc}}_strip_cfg.flags.invert_out = false;

    led_strip_rmt_config_t {{prefix_lc}}_rmt_cfg = {};
    {{prefix_lc}}_rmt_cfg.clk_src = RMT_CLK_SRC_DEFAULT;
    {{prefix_lc}}_rmt_cfg.resolution_hz = {{prefix}}_RMT_RES_HZ;
    {{prefix_lc}}_rmt_cfg.flags.with_dma = false;

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&{{prefix_lc}}_strip_cfg, &{{prefix_lc}}_rmt_cfg, &s_{{prefix_lc}}_strip));
    led_strip_clear(s_{{prefix_lc}}_strip);
    ESP_LOGI(TAG, "WS2812 strip {{prefix_lc}} initialized (GPIO %d, %d LEDs)", {{prefix}}_LED_GPIO, {{prefix}}_NUM_LEDS);
}
