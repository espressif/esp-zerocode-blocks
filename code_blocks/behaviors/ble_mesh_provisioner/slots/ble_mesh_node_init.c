{
    /* {{prefix_lc}}: this device forms the network. */
    static const uint8_t {{prefix_lc}}_match[2] = { ZC_MESH_UUID_B0, ZC_MESH_UUID_B1 };

    zc_prov_parse_app_key();
    /* A provisioner must not also sit there beaconing for a provisioner of its
     * own — the framework's node advertising is switched off here. */
    s_advertise_unprovisioned = false;
    esp_ble_mesh_register_config_client_callback(zc_prov_cfg_client_cb);

    esp_err_t {{prefix_lc}}_err = esp_ble_mesh_provisioner_set_dev_uuid_match(
        {{prefix_lc}}_match, sizeof({{prefix_lc}}_match), 0x00, false);
    if ({{prefix_lc}}_err != ESP_OK) {
        ESP_LOGE(TAG, "provisioner: could not set the device UUID filter (%s)",
                 esp_err_to_name({{prefix_lc}}_err));
    }
    {{prefix_lc}}_err = esp_ble_mesh_provisioner_prov_enable(ZC_MESH_BEARERS);
    if ({{prefix_lc}}_err != ESP_OK) {
        ESP_LOGE(TAG, "provisioner: could not start provisioning (%s)",
                 esp_err_to_name({{prefix_lc}}_err));
    }
    /* Adding the local application key is what eventually gives this device's
     * own client models a key to send with — see the ADD_LOCAL_APP_KEY event. */
    {{prefix_lc}}_err = esp_ble_mesh_provisioner_add_local_app_key(
        s_prov_app_key, s_prov_net_idx, s_prov_app_idx);
    if ({{prefix_lc}}_err != ESP_OK) {
        ESP_LOGE(TAG, "provisioner: could not add the local application key (%s)",
                 esp_err_to_name({{prefix_lc}}_err));
    }
    ESP_LOGI(TAG, "provisioner active — adding devices whose UUID starts 0x%02X%02X "
                  "into group 0x%04x (up to %d nodes)",
             ZC_MESH_UUID_B0, ZC_MESH_UUID_B1, (unsigned)ZC_PROV_GROUP_ADDR, ZC_PROV_MAX_NODES);
}
