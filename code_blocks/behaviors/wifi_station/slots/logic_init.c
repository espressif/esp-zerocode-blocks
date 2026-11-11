{
#if CONFIG_SOC_WIFI_SUPPORTED || CONFIG_SOC_WIRELESS_HOST_SUPPORTED
    nvs_handle_t h;
    if (nvs_open("zc_wifi", NVS_READONLY, &h) == ESP_OK) {
        size_t n = sizeof(s_{{prefix_lc}}_ssid);
        nvs_get_str(h, "ssid", s_{{prefix_lc}}_ssid, &n);
        n = sizeof(s_{{prefix_lc}}_pass);
        nvs_get_str(h, "pass", s_{{prefix_lc}}_pass, &n);
        nvs_close(h);
    }

    esp_console_cmd_t set_c = {};
    set_c.command = "wifi-set";
    set_c.help = "Set the Wi-Fi network: wifi-set <ssid> [password]";
    set_c.func = &{{prefix_lc}}_set_cmd;
    esp_console_cmd_register(&set_c);
    esp_console_cmd_t setup_c = {};
    setup_c.command = "wifi-setup";
    setup_c.help = "Set the Wi-Fi network, guided: asks for the network name, then the password";
    setup_c.func = &{{prefix_lc}}_setup_cmd;
    esp_console_cmd_register(&setup_c);
    esp_console_cmd_t st_c = {};
    st_c.command = "wifi-status";
    st_c.help = "Wi-Fi network, connection, address and signal";
    st_c.func = &{{prefix_lc}}_status_cmd;
    esp_console_cmd_register(&st_c);

    /* Frameworks init before behaviors: an initialised driver means one of
     * them owns the radio. */
    wifi_mode_t mode;
    if (esp_wifi_get_mode(&mode) != ESP_ERR_WIFI_NOT_INIT) {
        ESP_LOGI(TAG, "wifi: owned by another part of the firmware - standing aside");
        esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, {{prefix_lc}}_event_cb, NULL);
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, {{prefix_lc}}_event_cb, NULL);
        return;
    }

    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "wifi: esp_netif_init: %s", esp_err_to_name(err));
        return;
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "wifi: event loop: %s", esp_err_to_name(err));
        return;
    }
    if (esp_netif_get_handle_from_ifkey("WIFI_STA_DEF") == NULL) esp_netif_create_default_wifi_sta();
    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&wcfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "wifi: esp_wifi_init: %s", esp_err_to_name(err));
        return;
    }
    s_{{prefix_lc}}_owned = true;

    esp_timer_create_args_t ta = {};
    ta.callback = &{{prefix_lc}}_retry_cb;
    ta.dispatch_method = ESP_TIMER_TASK;
    ta.name = "wifi_retry";
    esp_timer_create(&ta, &s_{{prefix_lc}}_retry);
    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, {{prefix_lc}}_event_cb, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, {{prefix_lc}}_event_cb, NULL);

    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(WIFI_MODE_STA);
    if (s_{{prefix_lc}}_ssid[0] != 0) {
        {{prefix_lc}}_apply_config();
    } else {
        ESP_LOGW(TAG, "wifi: no network set - run: wifi-setup (guided), or wifi-set <ssid> [password]");
    }
    esp_wifi_start();
    if (s_{{prefix_lc}}_ssid[0] != 0) esp_wifi_connect();
#else
    ESP_LOGW(TAG, "wifi: this chip has no Wi-Fi - behaviors/wifi_station does nothing");
#endif
}
