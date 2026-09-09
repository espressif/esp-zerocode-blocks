/* air_quality_param (AirQuality index) and tvoc_param have no ZCL cluster —
 * not forwarded to Zigbee (see comment above). */
{{#if cfg.co2_ppm_param}}
if (param_id == {{cfg.co2_ppm_param}} && s_{{prefix_lc}}_zb_endpoint) {
    /* Driver carries ppm; ZCL CO2 MeasuredValue is a float fraction of one. */
    float v = (float)val.u16 * 1e-6f;
    if (esp_zigbee_lock_acquire(portMAX_DELAY)) {
        (void)ezb_zcl_set_attr_value(s_{{prefix_lc}}_zb_endpoint,
            EZB_ZCL_CLUSTER_ID_CARBON_DIOXIDE_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER,
            EZB_ZCL_ATTR_CARBON_DIOXIDE_MEASUREMENT_MEASURED_VALUE_ID,
            EZB_ZCL_STD_MANUF_CODE, &v, false);
        esp_zigbee_lock_release();
    }
}
{{/if}}
{{#if cfg.pm25_param}}
if (param_id == {{cfg.pm25_param}} && s_{{prefix_lc}}_zb_endpoint) {
    /* Driver carries µg/m³; ZCL PM2.5 MeasuredValue is a float in µg/m³
     * (ecosystem convention; the header's generic 0..1 min/max bounds are
     * left at their NaN defaults so no range check applies). */
    float v = (float)val.u16;
    if (esp_zigbee_lock_acquire(portMAX_DELAY)) {
        (void)ezb_zcl_set_attr_value(s_{{prefix_lc}}_zb_endpoint,
            EZB_ZCL_CLUSTER_ID_PM2_5_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER,
            EZB_ZCL_ATTR_PM2_5_MEASUREMENT_MEASURED_VALUE_ID,
            EZB_ZCL_STD_MANUF_CODE, &v, false);
        esp_zigbee_lock_release();
    }
}
{{/if}}
