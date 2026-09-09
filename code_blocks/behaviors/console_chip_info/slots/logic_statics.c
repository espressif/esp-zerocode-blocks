static int {{prefix_lc}}_cmd(int argc, char **argv)
{
    esp_chip_info_t info = {};
    esp_chip_info(&info);
    uint8_t mac[6] = {};
    esp_efuse_mac_get_default(mac);
    const char *model = "unknown";
    switch (info.model) {
        case CHIP_ESP32:   model = "ESP32";    break;
        case CHIP_ESP32S2: model = "ESP32-S2"; break;
        case CHIP_ESP32S3: model = "ESP32-S3"; break;
        case CHIP_ESP32C3: model = "ESP32-C3"; break;
        case CHIP_ESP32C6: model = "ESP32-C6"; break;
        case CHIP_ESP32H2: model = "ESP32-H2"; break;
        default: break;
    }
    printf("chip:     %s rev v%d.%d  cores=%d\n",
           model, info.revision / 100, info.revision % 100, info.cores);
    printf("features: %s%s%s%s\n",
           (info.features & CHIP_FEATURE_WIFI_BGN)   ? "Wi-Fi "   : "",
           (info.features & CHIP_FEATURE_BT)         ? "BT "      : "",
           (info.features & CHIP_FEATURE_BLE)        ? "BLE "     : "",
           (info.features & CHIP_FEATURE_IEEE802154) ? "802.15.4 ": "");
    printf("mac:      %02x:%02x:%02x:%02x:%02x:%02x\n",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return 0;
}
