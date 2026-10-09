static int {{prefix_lc}}_cmd(int argc, char **argv)
{
    esp_chip_info_t info = {};
    esp_chip_info(&info);
    uint8_t mac[6] = {};
    esp_efuse_mac_get_default(mac);

    /* The bootloader refuses an image built for another chip, so the build
     * target IS the running chip: "esp32s31" -> "ESP32-S31". */
    char model[16];
    const char *target = CONFIG_IDF_TARGET;
    size_t n = 0;
    for (size_t i = 0; target[i] != '\0' && n + 2 < sizeof(model); i++) {
        if (i == 5) {
            model[n++] = '-';
        }
        model[n++] = (char)toupper((unsigned char)target[i]);
    }
    model[n] = '\0';

    printf("chip:     %s rev v%d.%d  cores=%d\n",
           model, info.revision / 100, info.revision % 100, info.cores);
    printf("features: %s%s%s%s%s%s\n",
           (info.features & CHIP_FEATURE_WIFI_BGN)   ? "Wi-Fi "          : "",
           (info.features & CHIP_FEATURE_BT)         ? "BT "             : "",
           (info.features & CHIP_FEATURE_BLE)        ? "BLE "            : "",
           (info.features & CHIP_FEATURE_IEEE802154) ? "802.15.4 "       : "",
           (info.features & CHIP_FEATURE_EMB_FLASH)  ? "embedded-flash " : "",
           (info.features & CHIP_FEATURE_EMB_PSRAM)  ? "embedded-PSRAM " : "");
    printf("mac:      %02x:%02x:%02x:%02x:%02x:%02x\n",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return 0;
}
