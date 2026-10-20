static uint16_t s_{{prefix_lc}}_endpoint_id = 0;

#ifndef ZC_SWITCH_PRESS_HELPER
#define ZC_SWITCH_PRESS_HELPER
/* The Switch cluster is CODE-DRIVEN in esp_matter 1.6: reads come from CHIP's
 * SwitchCluster object, and Apple/Google Home act on its InitialPress /
 * ShortRelease EVENTS, not on CurrentPosition. esp_matter::attribute::update()
 * reaches neither. Report one complete momentary press on the cluster itself:
 * position goes up, InitialPress, position back to 0, ShortRelease (sent only
 * when the endpoint has MomentarySwitchRelease; harmless otherwise). Takes the
 * stack lock (scoped; a no-op when the caller already holds it). */
static void zc_switch_press(uint16_t ep, uint8_t position)
{
    esp_matter::lock::ScopedChipStackLock lk(portMAX_DELAY);
    chip::app::Clusters::SwitchCluster *c = chip::app::Clusters::Switch::FindClusterOnEndpoint(ep);
    if (c) {
        if (c->SetCurrentPosition(position) == CHIP_NO_ERROR) {
            c->OnInitialPress(position);
            (void)c->SetCurrentPosition(0);
            c->OnShortRelease(position);
        }
    } else {
        ESP_LOGW(TAG, "switch ep %u: cluster not registered yet, press dropped", ep);
    }
}
#endif
