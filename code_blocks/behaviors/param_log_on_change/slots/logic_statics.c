static void {{prefix_lc}}_log_cb(
    app_driver_param_id_t param_id, app_driver_param_val_t val,
    app_driver_handle_t source, void *ctx)
{
    ESP_LOGI(TAG, "{{prefix_lc}}: param %d changed (b=%d u8=%u i16=%d u16=%u u32=%lu) from source %u",
             (int)param_id, val.b, val.u8, val.i16, val.u16, (unsigned long)val.u32, source);
}
