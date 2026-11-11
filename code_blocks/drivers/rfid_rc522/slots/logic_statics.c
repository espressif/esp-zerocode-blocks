/* {{prefix_lc}}: RC522 card reader (i2c) */
#if defined({{prefix}}_RFID_IF_spi) && {{prefix}}_RFID_SPI_CS < 0
#error "rfid_rc522 {{prefix}}: interface spi needs spi_cs"
#endif
static rc522_driver_handle_t s_{{prefix_lc}}_rfid_drv = NULL;
static rc522_handle_t s_{{prefix_lc}}_rfid = NULL;
static rfid_card_cb_t s_{{prefix_lc}}_rfid_cb = NULL;
static void *s_{{prefix_lc}}_rfid_ctx = NULL;

extern "C" esp_err_t rfid_on_card(rfid_card_cb_t cb, void *ctx)
{
    s_{{prefix_lc}}_rfid_ctx = ctx;
    s_{{prefix_lc}}_rfid_cb = cb;
    return ESP_OK;
}

/* A card that reached ACTIVE arrived; one that drops back to IDLE from ACTIVE
 * or beyond was taken away (the library's own definition of removal). */
static void {{prefix_lc}}_rfid_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)id;
    const rc522_picc_state_changed_event_t *ev = (const rc522_picc_state_changed_event_t *)data;
    const rc522_picc_t *picc = ev->picc;
    bool arrived = picc->state == RC522_PICC_STATE_ACTIVE;
    bool removed = picc->state == RC522_PICC_STATE_IDLE && ev->old_state >= RC522_PICC_STATE_ACTIVE;
    if (!arrived && !removed) return;
    char hex[3 * RC522_PICC_UID_SIZE_MAX + 1] = {};
    rc522_picc_uid_to_str(&picc->uid, hex, sizeof(hex));
    ESP_LOGI(TAG, "{{prefix_lc}}: card %s %s", hex, arrived ? "on the reader" : "taken away");
{{#if cfg.present_param}}    {
        app_driver_param_val_t pv = {};
        pv.b = arrived;
        app_driver_set_param({{cfg.present_param}}, pv, APP_DRIVER_SOURCE_LOCAL);
    }
{{/if}}{{#if cfg.card_param}}    if (arrived) {
        app_driver_param_val_t cv = {};
        cv.u8 = 1;
        app_driver_fire_event({{cfg.card_param}}, cv, APP_DRIVER_SOURCE_LOCAL);
    }
{{/if}}    rfid_card_cb_t cb = s_{{prefix_lc}}_rfid_cb;
    if (cb) cb(picc->uid.value, picc->uid.length, arrived, s_{{prefix_lc}}_rfid_ctx);
}
