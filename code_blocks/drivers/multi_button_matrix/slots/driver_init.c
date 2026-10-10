{
    for (int r = 0; r < {{prefix}}_MATRIX_ROWS; ++r) {
        gpio_config_t row_cfg = {};
        row_cfg.pin_bit_mask = (1ULL << s_{{prefix_lc}}_rows[r]);
        row_cfg.mode = GPIO_MODE_OUTPUT_OD;
        row_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
        row_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
        row_cfg.intr_type = GPIO_INTR_DISABLE;
        gpio_config(&row_cfg);
        gpio_set_level((gpio_num_t)s_{{prefix_lc}}_rows[r], 1);
    }
    for (int c = 0; c < {{prefix}}_MATRIX_COLS; ++c) {
        gpio_config_t col_cfg = {};
        col_cfg.pin_bit_mask = (1ULL << s_{{prefix_lc}}_cols[c]);
        col_cfg.mode = GPIO_MODE_INPUT;
        col_cfg.pull_up_en = GPIO_PULLUP_ENABLE;
        col_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
        col_cfg.intr_type = GPIO_INTR_DISABLE;
        gpio_config(&col_cfg);
    }
    ESP_LOGI(TAG, "Button matrix {{prefix_lc}}: %dx%d initialized", {{prefix}}_MATRIX_ROWS, {{prefix}}_MATRIX_COLS);
}
