#if {{prefix}}_CMD_SCALE < 1
#error "console_param_cmd {{prefix}}: scale must be at least 1"
#endif
#if {{prefix}}_CMD_MIN > {{prefix}}_CMD_MAX
#error "console_param_cmd {{prefix}}: min must not exceed max"
#endif

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
    } else {
        /* The typed value in the owner's units; the param holds it times scale. */
        char *end = NULL;
        long long raw;
        if ({{prefix}}_CMD_SCALE > 1) {
            double d = strtod(argv[1], &end);
            raw = llround(d * {{prefix}}_CMD_SCALE);
        } else {
            raw = strtoll(argv[1], &end, 0);
        }
        if (end == argv[1] || *end != '\0') { printf("error: '%s' is not a number\n", argv[1]); return 1; }
        long long lo, hi;
        if (!strcmp(kind, "u8"))       { lo = 0; hi = 255; }
        else if (!strcmp(kind, "u16")) { lo = 0; hi = 65535; }
        else if (!strcmp(kind, "i16")) { lo = -32768; hi = 32767; }
        else                           { lo = 0; hi = 4294967295LL; }
        if ({{prefix}}_CMD_MIN != 0 || {{prefix}}_CMD_MAX != 0) {
            if ({{prefix}}_CMD_MIN > lo) lo = {{prefix}}_CMD_MIN;
            if ({{prefix}}_CMD_MAX < hi) hi = {{prefix}}_CMD_MAX;
        }
        if (raw < lo || raw > hi) {
            if ({{prefix}}_CMD_SCALE > 1) {
                printf("error: %s must be %.2f to %.2f\n", {{prefix}}_CMD_NAME,
                       (double)lo / {{prefix}}_CMD_SCALE, (double)hi / {{prefix}}_CMD_SCALE);
            } else {
                printf("error: %s must be %lld to %lld\n", {{prefix}}_CMD_NAME, lo, hi);
            }
            return 1;
        }
        if (!strcmp(kind, "u8"))       v.u8 = (uint8_t)raw;
        else if (!strcmp(kind, "u16")) v.u16 = (uint16_t)raw;
        else if (!strcmp(kind, "i16")) v.i16 = (int16_t)raw;
        else                           v.u32 = (uint32_t)raw;
    }
    /* mode is a compile-time constant; the compiler folds the strcmp. An event
     * param must be FIRED — set_param would swallow every repeat of the same
     * value, and "ring again" is exactly what a bench command is for. */
    esp_err_t err = !strcmp("{{cfg.mode}}", "event")
        ? app_driver_fire_event({{cfg.target_param}}, v, APP_DRIVER_SOURCE_LOCAL)
        : app_driver_set_param({{cfg.target_param}}, v, APP_DRIVER_SOURCE_LOCAL);
    if (err == ESP_ERR_NOT_ALLOWED) { printf("refused: the product does not allow that now\n"); return 1; }
    if (err != ESP_OK) { printf("error: %s\n", esp_err_to_name(err)); return 1; }
    printf("OK: %s <- %s\n", {{prefix}}_CMD_NAME, argv[1]);
    return 0;
}
