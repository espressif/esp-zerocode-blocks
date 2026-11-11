/* NODE_PROV_COMPLETE fires both when a provisioner adds the node and on every
 * boot of a node already provisioned (the stack replays it when it restores the
 * network from flash), so it alone covers "online". RESET is the node being
 * removed from the mesh — back to waiting to be set up. */
if (event == ESP_BLE_MESH_NODE_PROV_LINK_OPEN_EVT) {
    app_driver_param_val_t v = { .u8 = 3 };
    app_driver_set_param({{cfg.pattern_param}}, v, APP_DRIVER_SOURCE_LOCAL);
}
if (event == ESP_BLE_MESH_NODE_PROV_LINK_CLOSE_EVT && !esp_ble_mesh_node_is_provisioned()) {
    /* The link closed without provisioning us (abandoned or failed). */
    app_driver_param_val_t v = { .u8 = 2 };
    app_driver_set_param({{cfg.pattern_param}}, v, APP_DRIVER_SOURCE_LOCAL);
}
if (event == ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT) {
    app_driver_param_val_t v = { .u8 = 1 };
    app_driver_set_param({{cfg.pattern_param}}, v, APP_DRIVER_SOURCE_LOCAL);
}
if (event == ESP_BLE_MESH_NODE_PROV_RESET_EVT) {
    app_driver_param_val_t v = { .u8 = 2 };
    app_driver_set_param({{cfg.pattern_param}}, v, APP_DRIVER_SOURCE_LOCAL);
}
