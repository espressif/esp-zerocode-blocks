using namespace chip::DeviceLayer;
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
    case DeviceEventType::kFabricRemoved: {
        app_driver_param_val_t v = { .u8 = 2 };
        app_driver_set_param({{cfg.pattern_param}}, v, APP_DRIVER_SOURCE_LOCAL);
        break;
    }
    case DeviceEventType::kInternetConnectivityChange: {
        bool online = (event->InternetConnectivityChange.IPv6 == kConnectivity_Established) ||
                      (event->InternetConnectivityChange.IPv4 == kConnectivity_Established);
        app_driver_param_val_t v = { .u8 = (uint8_t)(online ? 1 : 4) };
        app_driver_set_param({{cfg.pattern_param}}, v, APP_DRIVER_SOURCE_LOCAL);
        break;
    }
    default:
        break;
}
