{
    const char *{{prefix_lc}}_edge = {{prefix}}_PC_EDGE_STR;
    const char *{{prefix_lc}}_pull = {{prefix}}_PC_PULL_STR;
    gpio_config_t {{prefix_lc}}_io = {};
    {{prefix_lc}}_io.pin_bit_mask = 1ULL << {{prefix}}_PC_GPIO;
    {{prefix_lc}}_io.mode = GPIO_MODE_INPUT;
    {{prefix_lc}}_io.pull_up_en = strcmp({{prefix_lc}}_pull, "up") == 0 ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
    {{prefix_lc}}_io.pull_down_en = strcmp({{prefix_lc}}_pull, "down") == 0 ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE;
    {{prefix_lc}}_io.intr_type = strcmp({{prefix_lc}}_edge, "rising") == 0 ? GPIO_INTR_POSEDGE
                               : strcmp({{prefix_lc}}_edge, "any") == 0 ? GPIO_INTR_ANYEDGE : GPIO_INTR_NEGEDGE;
    esp_err_t {{prefix_lc}}_err = gpio_config(&{{prefix_lc}}_io);
    if ({{prefix_lc}}_err == ESP_OK) {
        /* Another block may have installed the shared ISR service already. */
        esp_err_t svc = gpio_install_isr_service(0);
        if (svc != ESP_OK && svc != ESP_ERR_INVALID_STATE) {{prefix_lc}}_err = svc;
    }
    if ({{prefix_lc}}_err == ESP_OK) {{prefix_lc}}_err = gpio_isr_handler_add((gpio_num_t){{prefix}}_PC_GPIO, {{prefix_lc}}_pc_isr, NULL);
    if ({{prefix_lc}}_err != ESP_OK) {
        ESP_LOGE(TAG, "pulse counter {{prefix_lc}}: GPIO%d not usable (%s) - not counting", {{prefix}}_PC_GPIO, esp_err_to_name({{prefix_lc}}_err));
    } else {
        const esp_timer_create_args_t {{prefix_lc}}_pc_args = {
            .callback = &{{prefix_lc}}_pc_publish_cb,
            .arg = NULL,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "{{prefix_lc}}_pc",
            .skip_unhandled_events = true,
        };
        esp_timer_handle_t {{prefix_lc}}_pc_timer = NULL;
        ESP_ERROR_CHECK(esp_timer_create(&{{prefix_lc}}_pc_args, &{{prefix_lc}}_pc_timer));
        ESP_ERROR_CHECK(esp_timer_start_periodic({{prefix_lc}}_pc_timer, (uint64_t){{prefix}}_PC_PUBLISH_MS * 1000ULL));
        ESP_LOGI(TAG, "pulse counter {{prefix_lc}}: GPIO%d, %s edge, pull %s", {{prefix}}_PC_GPIO, {{prefix_lc}}_edge, {{prefix_lc}}_pull);
    }
}
