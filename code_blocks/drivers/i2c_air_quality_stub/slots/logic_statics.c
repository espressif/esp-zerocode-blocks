/* {{prefix_lc}}: air-quality poll task.
 *
 * NO SENSOR PROTOCOL IS IMPLEMENTED HERE — this block brings up the bus and
 * the poll timer, nothing more. It therefore publishes NOTHING.
 *
 * It used to publish a fixed air-quality enum (and CO2 ppm) on every poll,
 * which reads exactly like a real measurement: the controller shows a
 * plausible constant "good air / 400ppm" forever and no gate can tell the
 * difference. Fail CLOSED instead — a missing reading is visible, a fake one
 * is not. Wire a real sensor driver before this product measures anything.
 */
static void {{prefix_lc}}_poll_cb(void *arg)
{
    static bool warned = false;
    static uint32_t ticks = 0;
    if (!warned || (++ticks % 60) == 0) {
        warned = true;
        ESP_LOGE("{{prefix_lc}}", "no air-quality sensor protocol implemented — publishing NOTHING");
    }
}
