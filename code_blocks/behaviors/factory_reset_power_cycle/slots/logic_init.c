{
    nvs_handle_t {{prefix_lc}}_pc_h;
    if (nvs_open({{prefix}}_PC_NVS_NS, NVS_READWRITE, &{{prefix_lc}}_pc_h) == ESP_OK) {
        uint8_t {{prefix_lc}}_pc = 0;
        nvs_get_u8({{prefix_lc}}_pc_h, {{prefix}}_PC_NVS_KEY, &{{prefix_lc}}_pc);
        /* Only a power-on (or the reset pin) is a deliberate power cycle.
         * Panics, watchdogs, brownouts and esp_restart() (OTA, a framework's
         * own reboot) also come back through here, and counting them meant a
         * crash loop or a weak supply could erase NVS — every fabric and
         * credential — with nobody touching the device. Those boots leave the
         * count as it is; the stable-boot timer below still clears it. The EN
         * pin reports POWERON on every current chip (ESP_RST_EXT is kept for
         * one that reports it). A flick too short for the rail to drop below
         * power-on reset reads as a brownout and is not counted: cycles must
         * be off long enough to fully power down (about a second). */
        esp_reset_reason_t {{prefix_lc}}_rr = esp_reset_reason();
        if ({{prefix_lc}}_rr == ESP_RST_POWERON || {{prefix_lc}}_rr == ESP_RST_EXT) {
            {{prefix_lc}}_pc++;
        } else {
            ESP_LOGI(TAG, "{{prefix_lc}}: reset reason %d is not a power cycle — not counted", (int){{prefix_lc}}_rr);
        }
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
