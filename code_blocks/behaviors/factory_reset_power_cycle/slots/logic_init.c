{
    nvs_handle_t {{prefix_lc}}_pc_h;
    if (nvs_open({{prefix}}_PC_NVS_NS, NVS_READWRITE, &{{prefix_lc}}_pc_h) == ESP_OK) {
        uint8_t {{prefix_lc}}_pc = 0;
        nvs_get_u8({{prefix_lc}}_pc_h, {{prefix}}_PC_NVS_KEY, &{{prefix_lc}}_pc);
        {{prefix_lc}}_pc++;
        ESP_LOGI(TAG, "{{prefix_lc}}: power-cycle count = %u/%u", {{prefix_lc}}_pc, {{prefix}}_PC_THRESHOLD);
        if ({{prefix_lc}}_pc >= {{prefix}}_PC_THRESHOLD) {
            ESP_LOGW(TAG, "{{prefix_lc}}: threshold reached — erasing NVS and rebooting");
            nvs_close({{prefix_lc}}_pc_h);
            nvs_flash_erase();
            esp_restart();
        }
        nvs_set_u8({{prefix_lc}}_pc_h, {{prefix}}_PC_NVS_KEY, {{prefix_lc}}_pc);
        nvs_commit({{prefix_lc}}_pc_h);
        nvs_close({{prefix_lc}}_pc_h);
    }
    const esp_timer_create_args_t {{prefix_lc}}_pc_args = {
        .callback = &{{prefix_lc}}_pc_clear_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "{{prefix_lc}}_pc",
        .skip_unhandled_events = true,
    };
    esp_timer_handle_t {{prefix_lc}}_pc_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&{{prefix_lc}}_pc_args, &{{prefix_lc}}_pc_timer));
    ESP_ERROR_CHECK(esp_timer_start_once({{prefix_lc}}_pc_timer, (uint64_t){{prefix}}_PC_STABLE_MS * 1000ULL));
}
