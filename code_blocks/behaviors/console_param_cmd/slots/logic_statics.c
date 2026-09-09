static int {{prefix_lc}}_cmd(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage: %s <value>\n", {{prefix}}_CMD_NAME);
        return 1;
    }
    app_driver_param_val_t v = {};
    const char *kind = "{{cfg.value_kind}}";
    if (!strcmp(kind, "bool")) {
        v.b = (!strcasecmp(argv[1], "on")) || (!strcasecmp(argv[1], "true")) || (!strcmp(argv[1], "1"));
        printf("OK: {{cfg.target_param}} <- %s\n", v.b ? "true" : "false");
    } else if (!strcmp(kind, "u8")) {
        unsigned long u = strtoul(argv[1], NULL, 0);
        if (u > 255UL) { printf("error: u8 out of range\n"); return 1; }
        v.u8 = (uint8_t)u;
        printf("OK: {{cfg.target_param}} <- %u\n", (unsigned)v.u8);
    } else if (!strcmp(kind, "u16")) {
        unsigned long u = strtoul(argv[1], NULL, 0);
        if (u > 65535UL) { printf("error: u16 out of range\n"); return 1; }
        v.u16 = (uint16_t)u;
        printf("OK: {{cfg.target_param}} <- %u\n", (unsigned)v.u16);
    } else if (!strcmp(kind, "i16")) {
        long s = strtol(argv[1], NULL, 0);
        if (s < -32768L || s > 32767L) { printf("error: i16 out of range\n"); return 1; }
        v.i16 = (int16_t)s;
        printf("OK: {{cfg.target_param}} <- %d\n", (int)v.i16);
    } else { /* u32 */
        v.u32 = (uint32_t)strtoul(argv[1], NULL, 0);
        printf("OK: {{cfg.target_param}} <- %lu\n", (unsigned long)v.u32);
    }
    /* mode is a compile-time constant; the compiler folds the strcmp. An event
     * param must be FIRED — set_param would swallow every repeat of the same
     * value, and "ring again" is exactly what a bench command is for. */
    if (!strcmp("{{cfg.mode}}", "event")) {
        app_driver_fire_event({{cfg.target_param}}, v, APP_DRIVER_SOURCE_LOCAL);
    } else {
        app_driver_set_param({{cfg.target_param}}, v, APP_DRIVER_SOURCE_LOCAL);
    }
    return 0;
}
