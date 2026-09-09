static esp_rmaker_device_t *s_{{prefix_lc}}_device = NULL;
static esp_rmaker_param_t *s_{{prefix_lc}}_lux_param = NULL;

/* Driver param carries the Matter IlluminanceMeasurement spec encoding:
 * MeasuredValue = 10000 * log10(lux) + 1, 0 = "too low to measure".
 * Decode back to natural lux for RainMaker. */
static float {{prefix_lc}}_encoded_to_lux(uint16_t v)
{
    if (v == 0) return 0.0f;
    return powf(10.0f, ((float)v - 1.0f) / 10000.0f);
}
