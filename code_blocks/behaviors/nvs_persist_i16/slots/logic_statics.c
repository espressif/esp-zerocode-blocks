/* The listener only records the value; the write happens on the bus's poll
 * task (app_driver_poll_every), at most every {{prefix}}_PERSIST_MS. A flash
 * commit (tens of ms when NVS erases a page) used to run on whichever task
 * changed the param — Matter's, the shared esp_timer task — and a slider drag
 * wrote flash on every step; now a burst of changes is one write of the last
 * value. A power cut within that window keeps the value before it. */
#ifndef {{prefix}}_PERSIST_MS
#define {{prefix}}_PERSIST_MS 250
#endif
static portMUX_TYPE s_{{prefix_lc}}_persist_mux = portMUX_INITIALIZER_UNLOCKED;
static app_driver_param_val_t s_{{prefix_lc}}_pending = {};
static bool s_{{prefix_lc}}_dirty = false;

static void {{prefix_lc}}_persist_cb(
    app_driver_param_id_t param_id,
    app_driver_param_val_t val,
    app_driver_handle_t source,
    void *ctx)
{
    if (param_id != {{cfg.target_param}}) return;
    portENTER_CRITICAL(&s_{{prefix_lc}}_persist_mux);
    s_{{prefix_lc}}_pending = val;
    s_{{prefix_lc}}_dirty = true;
    portEXIT_CRITICAL(&s_{{prefix_lc}}_persist_mux);
}

static void {{prefix_lc}}_persist_write(void *arg)
{
    (void)arg;
    portENTER_CRITICAL(&s_{{prefix_lc}}_persist_mux);
    bool dirty = s_{{prefix_lc}}_dirty;
    app_driver_param_val_t val = s_{{prefix_lc}}_pending;
    s_{{prefix_lc}}_dirty = false;
    portEXIT_CRITICAL(&s_{{prefix_lc}}_persist_mux);
    if (!dirty) return;
    nvs_handle_t h;
    if (nvs_open({{prefix}}_NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_i16(h, {{prefix}}_NVS_KEY, val.i16);
    nvs_commit(h);
    nvs_close(h);
}
