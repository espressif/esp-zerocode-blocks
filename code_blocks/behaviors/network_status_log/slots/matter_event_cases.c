using namespace chip::DeviceLayer;
switch (event->Type) {
    case DeviceEventType::kCommissioningSessionStarted:
        ESP_LOGI(TAG, "{{prefix_lc}}: commissioning session started"); break;
    case DeviceEventType::kCommissioningSessionStopped:
        ESP_LOGI(TAG, "{{prefix_lc}}: commissioning session stopped"); break;
    case DeviceEventType::kCommissioningComplete:
        ESP_LOGI(TAG, "{{prefix_lc}}: commissioning complete — device is on a fabric"); break;
    case DeviceEventType::kFabricRemoved:
        ESP_LOGW(TAG, "{{prefix_lc}}: fabric removed"); break;
    case DeviceEventType::kInterfaceIpAddressChanged:
        ESP_LOGI(TAG, "{{prefix_lc}}: IP interface changed"); break;
    case DeviceEventType::kInternetConnectivityChange: {
        const char *v6 = (event->InternetConnectivityChange.IPv6 == kConnectivity_Established) ? "up" : "down";
        const char *v4 = (event->InternetConnectivityChange.IPv4 == kConnectivity_Established) ? "up" : "down";
        ESP_LOGI(TAG, "{{prefix_lc}}: connectivity v4=%s v6=%s", v4, v6);
        break;
    }
    case DeviceEventType::kThreadConnectivityChange:
        ESP_LOGI(TAG, "{{prefix_lc}}: Thread connectivity change"); break;
    case DeviceEventType::kWiFiConnectivityChange:
        ESP_LOGI(TAG, "{{prefix_lc}}: Wi-Fi connectivity change"); break;
    default:
        break;
}
