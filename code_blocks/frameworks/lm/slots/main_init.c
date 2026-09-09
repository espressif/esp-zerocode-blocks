/* Degrades rather than aborts: a part without PSRAM cannot allocate the KV
 * cache, and a device that still switches relays is better than one that
 * refuses to boot. app_lm logs the reason. */
app_lm_init();
