{
    nvs_handle_t {{prefix_lc}}_h;
    esp_err_t {{prefix_lc}}_err = nvs_open("{{cfg.nvs_namespace}}", NVS_READWRITE, &{{prefix_lc}}_h);
    if ({{prefix_lc}}_err == ESP_OK) {
        uint32_t {{prefix_lc}}_count = 0;
        nvs_get_u32({{prefix_lc}}_h, "{{cfg.nvs_key}}", &{{prefix_lc}}_count);
        {{prefix_lc}}_count++;
        nvs_set_u32({{prefix_lc}}_h, "{{cfg.nvs_key}}", {{prefix_lc}}_count);
        nvs_commit({{prefix_lc}}_h);
        nvs_close({{prefix_lc}}_h);
        ESP_LOGI(TAG, "Boot #%lu", (unsigned long){{prefix_lc}}_count);
    } else {
        ESP_LOGW(TAG, "boot_count: nvs_open failed (%s)", esp_err_to_name({{prefix_lc}}_err));
    }
}
