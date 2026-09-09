/* {{prefix_lc}}: I2C temp/humidity sensor poll task.
 *
 * NO SENSOR PROTOCOL IS IMPLEMENTED HERE — this block brings up the I2C bus
 * and the poll timer, nothing more. It therefore publishes NOTHING and says
 * so, loudly and periodically.
 *
 * It used to publish a fixed 25.00C / 55.00%RH, which is worse than useless:
 * a fabricated reading is indistinguishable from a real one, so Home
 * Assistant / the controller shows a plausible constant temperature forever
 * and no gate can tell. Fail CLOSED instead — a missing reading is visible,
 * a fake one is not.
 *
 * For real hardware use a real-sensor block: drivers/sht4x, drivers/sht3x,
 * drivers/aht21 or drivers/ds18b20. Do NOT hand-write raw i2c_master_*
 * register transactions in this slot — sensor protocols are easy to get wrong
 * under -Werror (compound-literal addresses, packed structs, byte order), and
 * a wrong one fabricates just as convincingly.
 */
static void {{prefix_lc}}_sensor_poll_cb(void *arg)
{
    static bool warned = false;
    static uint32_t ticks = 0;
    /* Once at first poll, then every ~5 min, so it stays visible in a long log
     * without drowning it. */
    if (!warned || (++ticks % 60) == 0) {
        warned = true;
        ESP_LOGE("{{prefix_lc}}", "no sensor protocol implemented — publishing NOTHING. "
                 "Use drivers/sht4x, sht3x, aht21 or ds18b20 for real readings.");
    }
}
