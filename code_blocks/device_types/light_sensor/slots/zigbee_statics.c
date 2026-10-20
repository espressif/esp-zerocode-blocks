static uint8_t s_{{prefix_lc}}_zb_endpoint = 0;

/* value_param is plain lux. ZCL Illuminance MeasuredValue is
 * 10000 * log10(lux) + 1 (0 = too low to measure, max 0xFFFE); encode here at
 * the framework boundary, since every light driver publishes linear lux. */
static uint16_t {{prefix_lc}}_zb_lux_encode(uint16_t lux)
{
    if (lux == 0) return 0;
    float v = 10000.0f * log10f((float)lux) + 1.0f;
    return v > 65534.0f ? 65534 : (uint16_t)v;
}
