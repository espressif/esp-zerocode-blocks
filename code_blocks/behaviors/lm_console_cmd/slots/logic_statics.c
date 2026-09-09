/* ── {{prefix}}: `{{cfg.command_name}} <words...>` → the language model ── */

static int {{prefix_lc}}_cmd(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage: {{cfg.command_name}} <command in plain words>\n");
        printf("  e.g. {{cfg.command_name}} turn on the kitchen light\n");
        return 1;
    }

    /* Rejoin the argv words the console split apart. */
    char line[APP_LM_MAX_CMD] = {0};
    size_t used = 0;
    for (int i = 1; i < argc && used < sizeof(line) - 1; i++) {
        int n = snprintf(line + used, sizeof(line) - used, "%s%s",
                         used ? " " : "", argv[i]);
        if (n < 0) {
            break;
        }
        used += (size_t)n;
    }

    /* Asynchronous by design: a decode is tens to hundreds of milliseconds and
     * the console task must not block on it. The result arrives in the log and
     * on the param bus. */
    esp_err_t err = app_lm_submit(line);
    if (err == ESP_ERR_INVALID_STATE) {
        printf("language model unavailable — it needs PSRAM; see the boot log\n");
        return 1;
    }
    if (err != ESP_OK) {
        printf("busy — a command is already being parsed\n");
        return 1;
    }
    printf("parsing \"%s\"...\n", line);
    return 0;
}
