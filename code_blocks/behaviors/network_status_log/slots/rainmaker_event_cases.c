if (event_base == NETWORK_PROV_EVENT) {
    switch (event_id) {
        case NETWORK_PROV_START:
            ESP_LOGI(TAG, "{{prefix_lc}}: provisioning started (use the ESP RainMaker app)"); break;
        case NETWORK_PROV_WIFI_CRED_RECV:
            ESP_LOGI(TAG, "{{prefix_lc}}: Wi-Fi credentials received"); break;
        case NETWORK_PROV_WIFI_CRED_FAIL:
            ESP_LOGW(TAG, "{{prefix_lc}}: Wi-Fi credentials failed"); break;
        case NETWORK_PROV_WIFI_CRED_SUCCESS:
            ESP_LOGI(TAG, "{{prefix_lc}}: Wi-Fi credentials OK"); break;
        case NETWORK_PROV_END:
            ESP_LOGI(TAG, "{{prefix_lc}}: provisioning complete"); break;
        default: break;
    }
}
if (event_base == RMAKER_COMMON_EVENT) {
    switch (event_id) {
        case RMAKER_MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "{{prefix_lc}}: RainMaker cloud connected"); break;
        case RMAKER_MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "{{prefix_lc}}: RainMaker cloud disconnected"); break;
        default: break;
    }
}
if (event_base == RMAKER_EVENT) {
    switch (event_id) {
        case RMAKER_EVENT_INIT_DONE:
            ESP_LOGI(TAG, "{{prefix_lc}}: RainMaker init done"); break;
        case RMAKER_EVENT_CLAIM_STARTED:
            ESP_LOGI(TAG, "{{prefix_lc}}: RainMaker claiming started"); break;
        case RMAKER_EVENT_CLAIM_SUCCESSFUL:
            ESP_LOGI(TAG, "{{prefix_lc}}: RainMaker claiming successful"); break;
        case RMAKER_EVENT_CLAIM_FAILED:
            ESP_LOGW(TAG, "{{prefix_lc}}: RainMaker claiming failed"); break;
        default: break;
    }
}
