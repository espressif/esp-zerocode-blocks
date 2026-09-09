/* {{prefix_lc}}: the OTA endpoint is registered via zigbee_endpoint_create
 * above; the image-query cycle is armed from the signal handler once the
 * device is on a network (native OTA commands require the started stack;
 * this slot renders before ezb_af_device_desc_register/esp_zigbee_start). */
ESP_LOGI(TAG, "{{prefix_lc}}: Zigbee OTA upgrade client ready (endpoint {{cfg.zb_ota_endpoint}})");
