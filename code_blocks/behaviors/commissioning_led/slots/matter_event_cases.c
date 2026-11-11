using namespace chip::DeviceLayer;
/* Paired / unpaired (fabric added or removed) is {{prefix_lc}}_matter_led_cb's,
 * from MATTER_COMMISSIONED; these are the moments in between. */
switch (event->Type) {
    case DeviceEventType::kCommissioningSessionStarted: {
        app_driver_param_val_t v = { .u8 = 3 };
        app_driver_set_param({{cfg.pattern_param}}, v, APP_DRIVER_SOURCE_LOCAL);
        break;
    }
    case DeviceEventType::kCommissioningComplete: {
        app_driver_param_val_t v = { .u8 = 1 };
        app_driver_set_param({{cfg.pattern_param}}, v, APP_DRIVER_SOURCE_LOCAL);
        break;
    }
    case DeviceEventType::kInternetConnectivityChange: {
        /* Online / lost network means something only on a paired device: Wi-Fi
         * comes up mid-pairing, before the device is on a fabric. */
        if (!{{prefix_lc}}_matter_paired()) break;
        bool online = (event->InternetConnectivityChange.IPv6 == kConnectivity_Established) ||
                      (event->InternetConnectivityChange.IPv4 == kConnectivity_Established);
        bool lost = (event->InternetConnectivityChange.IPv6 == kConnectivity_Lost) ||
                    (event->InternetConnectivityChange.IPv4 == kConnectivity_Lost);
        if (!online && !lost) break;
        app_driver_param_val_t v = { .u8 = (uint8_t)(online ? 1 : 4) };
        app_driver_set_param({{cfg.pattern_param}}, v, APP_DRIVER_SOURCE_LOCAL);
        break;
    }
    default:
        break;
}
