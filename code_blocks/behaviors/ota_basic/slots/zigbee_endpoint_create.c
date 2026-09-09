/* {{prefix_lc}}: dedicated native SDK 2.0 OTA Upgrade client endpoint.
 * dev_desc is supplied by the generated scaffold and registered after every
 * endpoint slot has contributed its descriptor. */
ezb_af_ep_config_t {{prefix_lc}}_ep_cfg = {
    .ep_id = {{cfg.zb_ota_endpoint}},
    .app_profile_id = EZB_AF_HA_PROFILE_ID,
    /* No standard HA id exists for a bare OTA client endpoint. Preserve the
     * previous custom test-device id. */
    .app_device_id = 0xfff0,
    .app_device_version = 0,
};
ezb_zcl_ota_upgrade_cluster_client_config_t {{prefix_lc}}_ota_cfg = {
    .upgrade_server_id = EZB_ZCL_OTA_UPGRADE_UPGRADE_SERVER_ID_DEFAULT_VALUE,
    .file_offset = EZB_ZCL_OTA_UPGRADE_FILE_OFFSET_DEFAULT_VALUE,
    .image_upgrade_status = EZB_ZCL_OTA_UPGRADE_IMAGE_UPGRADE_STATUS_DEFAULT_VALUE,
    .manufacturer_id = {{prefix}}_ZB_OTA_MANUFACTURER_ID,
    .image_type_id = {{prefix}}_ZB_OTA_IMAGE_TYPE,
};
ezb_af_ep_desc_t {{prefix_lc}}_ep_desc =
    ezb_af_create_endpoint_desc(&{{prefix_lc}}_ep_cfg);
ezb_zcl_cluster_desc_t {{prefix_lc}}_basic_desc =
    ezb_zcl_basic_create_cluster_desc(NULL, EZB_ZCL_CLUSTER_SERVER);
ezb_zcl_cluster_desc_t {{prefix_lc}}_ota_desc =
    ezb_zcl_ota_upgrade_create_cluster_desc(&{{prefix_lc}}_ota_cfg,
                                            EZB_ZCL_CLUSTER_CLIENT);
if ({{prefix_lc}}_ep_desc == EZB_INVALID_AF_EP_DESC ||
    {{prefix_lc}}_basic_desc == EZB_INVALID_ZCL_CLUSTER_DESC ||
    {{prefix_lc}}_ota_desc == EZB_INVALID_ZCL_CLUSTER_DESC) {
    ESP_LOGE(TAG, "{{prefix_lc}}: failed to allocate OTA endpoint descriptors");
    return ESP_ERR_NO_MEM;
}
ESP_ERROR_CHECK(esp_zigbee_err_to_esp(ezb_zcl_ota_upgrade_cluster_desc_add_attr(
    {{prefix_lc}}_ota_desc, EZB_ZCL_ATTR_OTA_UPGRADE_CURRENT_FILE_VERSION_ID,
    &s_{{prefix_lc}}_current_file_version)));
ESP_ERROR_CHECK(esp_zigbee_err_to_esp(ezb_af_endpoint_add_cluster_desc(
    {{prefix_lc}}_ep_desc, {{prefix_lc}}_basic_desc)));
ESP_ERROR_CHECK(esp_zigbee_err_to_esp(ezb_af_endpoint_add_cluster_desc(
    {{prefix_lc}}_ep_desc,
    ezb_zcl_identify_create_cluster_desc(NULL, EZB_ZCL_CLUSTER_SERVER))));
ESP_ERROR_CHECK(esp_zigbee_err_to_esp(ezb_af_endpoint_add_cluster_desc(
    {{prefix_lc}}_ep_desc, {{prefix_lc}}_ota_desc)));
ESP_ERROR_CHECK(esp_zigbee_err_to_esp(ezb_af_device_add_endpoint_desc(
    dev_desc, {{prefix_lc}}_ep_desc)));
ESP_LOGI(TAG, "{{prefix_lc}}: Zigbee OTA Upgrade client on endpoint %d", {{cfg.zb_ota_endpoint}});
