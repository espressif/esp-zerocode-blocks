static char s_{{prefix_lc}}_state_topic[72] = {0};
static char s_{{prefix_lc}}_cmd_topic[72] = {0};
/* HA select options are "Mode 1" .. "Mode <mode_count>" — generated from
 * mode_count like the RainMaker bounds and the Zigbee state count, so the
 * three bindings offer the same modes (a fixed four used to disagree). */
#define ZC_{{prefix_lc}}_MODE_COUNT ({{cfg.mode_count}})
static void {{prefix_lc}}_publish_mode(uint8_t mode)
{
    if (mode >= ZC_{{prefix_lc}}_MODE_COUNT) return;
    char name[16];
    snprintf(name, sizeof(name), "Mode %u", (unsigned)mode + 1);
    esp_mqtt_client_publish(s_client, s_{{prefix_lc}}_state_topic, name, 0, 1, true);
}
