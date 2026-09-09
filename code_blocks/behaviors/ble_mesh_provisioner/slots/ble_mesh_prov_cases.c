/* {{prefix_lc}}: provisioner half of the provisioning lifecycle. */
if (event == ESP_BLE_MESH_PROVISIONER_RECV_UNPROV_ADV_PKT_EVT) {
    /* The UUID filter set at init means only this kit's own devices are
     * reported here, so hearing one is reason enough to add it. */
    esp_ble_mesh_unprov_dev_add_t {{prefix_lc}}_dev = {};
    memcpy({{prefix_lc}}_dev.addr, param->provisioner_recv_unprov_adv_pkt.addr, BD_ADDR_LEN);
    {{prefix_lc}}_dev.addr_type =
        (esp_ble_mesh_addr_type_t)param->provisioner_recv_unprov_adv_pkt.addr_type;
    memcpy({{prefix_lc}}_dev.uuid, param->provisioner_recv_unprov_adv_pkt.dev_uuid, 16);
    {{prefix_lc}}_dev.oob_info = param->provisioner_recv_unprov_adv_pkt.oob_info;
    {{prefix_lc}}_dev.bearer =
        (esp_ble_mesh_prov_bearer_t)param->provisioner_recv_unprov_adv_pkt.bearer;
    esp_err_t {{prefix_lc}}_err = esp_ble_mesh_provisioner_add_unprov_dev(
        &{{prefix_lc}}_dev,
        (esp_ble_mesh_dev_add_flag_t)(ADD_DEV_RM_AFTER_PROV_FLAG |
                                      ADD_DEV_START_PROV_NOW_FLAG |
                                      ADD_DEV_FLUSHABLE_DEV_FLAG));
    if ({{prefix_lc}}_err != ESP_OK) {
        ESP_LOGW(TAG, "provisioner: could not queue a device (%s)",
                 esp_err_to_name({{prefix_lc}}_err));
    }
}
if (event == ESP_BLE_MESH_PROVISIONER_PROV_COMPLETE_EVT) {
    zc_prov_node_t *{{prefix_lc}}_node = zc_prov_store(
        param->provisioner_prov_complete.device_uuid,
        param->provisioner_prov_complete.unicast_addr,
        param->provisioner_prov_complete.element_num);
    if (!{{prefix_lc}}_node) {
        ESP_LOGE(TAG, "provisioner: node table full (max_nodes = %d) — device 0x%04x is "
                      "on the network but will not be configured",
                 ZC_PROV_MAX_NODES, param->provisioner_prov_complete.unicast_addr);
    } else {
        ESP_LOGI(TAG, "provisioner: provisioned 0x%04x (%d element(s)) — configuring it",
                 {{prefix_lc}}_node->unicast, {{prefix_lc}}_node->elem_num);
        zc_prov_run_step({{prefix_lc}}_node);
    }
}
if (event == ESP_BLE_MESH_PROVISIONER_ADD_LOCAL_APP_KEY_COMP_EVT) {
    if (param->provisioner_add_app_key_comp.err_code == 0) {
        s_prov_app_idx = param->provisioner_add_app_key_comp.app_idx;
        /* Bind the key to THIS device's own models too. A provisioner that
         * only ever binds its children can configure a whole network and then
         * not be able to send a single message itself. */
        const uint16_t {{prefix_lc}}_local[] = {
            ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_CLI,
            ESP_BLE_MESH_MODEL_ID_GEN_LEVEL_CLI,
            ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_SRV,
            ESP_BLE_MESH_MODEL_ID_GEN_LEVEL_SRV,
        };
        for (size_t {{prefix_lc}}_i = 0; {{prefix_lc}}_i < ZC_MESH_COUNT({{prefix_lc}}_local); {{prefix_lc}}_i++) {
            esp_err_t {{prefix_lc}}_err = esp_ble_mesh_provisioner_bind_app_key_to_local_model(
                ZC_PROV_OWN_ADDR, s_prov_app_idx, {{prefix_lc}}_local[{{prefix_lc}}_i],
                ESP_BLE_MESH_CID_NVAL);
            if ({{prefix_lc}}_err != ESP_OK) {
                ESP_LOGW(TAG, "provisioner: could not bind the key to local model 0x%04x (%s)",
                         {{prefix_lc}}_local[{{prefix_lc}}_i], esp_err_to_name({{prefix_lc}}_err));
            }
        }
    } else {
        ESP_LOGE(TAG, "provisioner: adding the local application key failed (err %d)",
                 param->provisioner_add_app_key_comp.err_code);
    }
}
