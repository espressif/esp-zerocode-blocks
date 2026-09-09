static void {{prefix_lc}}_pc_clear_cb(void *arg)
{
    nvs_handle_t h;
    if (nvs_open({{prefix}}_PC_NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_u8(h, {{prefix}}_PC_NVS_KEY, 0);
    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "{{prefix_lc}}: stable boot — power-cycle counter cleared");
}
