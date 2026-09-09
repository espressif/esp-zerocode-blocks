/* ── {{prefix}}: `{{cfg.command_name}} <prompt...>` → generated text ── */

/* Called once per decoded token, on the inference worker. chunk is NOT
 * null-terminated — print exactly len bytes. */
static void {{prefix_lc}}_emit(const char *chunk, int len, void *arg)
{
    (void)arg;
    fwrite(chunk, 1, (size_t)len, stdout);
    fflush(stdout);        /* stream it; a buffered story arrives as a wall */
}

static int {{prefix_lc}}_cmd(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage: {{cfg.command_name}} <opening words>\n");
        printf("  e.g. {{cfg.command_name}} once upon a time there was a robot\n");
        return 1;
    }

    char prompt[APP_LM_MAX_CMD] = {0};
    size_t used = 0;
    for (int i = 1; i < argc && used < sizeof(prompt) - 1; i++) {
        int n = snprintf(prompt + used, sizeof(prompt) - used, "%s%s",
                         used ? " " : "", argv[i]);
        if (n < 0) {
            break;
        }
        used += (size_t)n;
    }

    esp_err_t err = app_lm_submit_story(prompt, {{cfg.max_tokens}},
                                        {{cfg.temperature_pct}} / 100.0f,
                                        {{prefix_lc}}_emit, NULL);
    if (err == ESP_ERR_NOT_SUPPORTED) {
        printf("story model not built into this firmware\n");
        return 1;
    }
    if (err == ESP_ERR_INVALID_STATE) {
        printf("language model unavailable — it needs PSRAM; see the boot log\n");
        return 1;
    }
    if (err != ESP_OK) {
        printf("busy — one generation at a time\n");
        return 1;
    }
    /* Output streams from the worker, so this returns immediately and the story
     * appears underneath the prompt. */
    printf("%s", prompt);
    return 0;
}
