static void {{prefix_lc}}_persist_cb(
    app_driver_param_id_t param_id, app_driver_param_val_t val,
    app_driver_handle_t source, void *ctx)
{
    if (param_id != {{cfg.target_param}}) return;
    nvs_handle_t h;
    if (nvs_open({{prefix}}_NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_u32(h, {{prefix}}_NVS_KEY, val.u32);
    nvs_commit(h);
    nvs_close(h);
}
