{
    tg_load_settings();
    tg_store_load();
    s_tg_in = xQueueCreate(8, sizeof(char *));
    s_tg_lock = xSemaphoreCreateMutex();
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) ESP_LOGE(TAG, "telegram: esp_netif_init: %s", esp_err_to_name(err));
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) ESP_LOGE(TAG, "telegram: event loop: %s", esp_err_to_name(err));
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, tg_net_cb, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_LOST_IP, tg_net_cb, NULL);
    /* TLS with the certificate bundle needs far more stack than a timer or
     * console task has: all network work happens on this task. */
    if (s_tg_in == NULL || s_tg_lock == NULL ||
        xTaskCreate(tg_task, "telegram", 10240, NULL, 4, &s_tg_task) != pdPASS) {
        ESP_LOGE(TAG, "telegram: could not start its task - messages will not be sent");
        s_tg_task = NULL;
        return;
    }

    const struct { const char *command; const char *help; esp_console_cmd_func_t func; } cmds[] = {
        { "tg-token", "Set the Telegram bot token: tg-token <token from @BotFather>", &tg_token_cmd },
        { "tg-chat",  "Set the Telegram chat id by hand: tg-chat <id>",               &tg_chat_cmd  },
        { "tg-learn", "Learn the chat id from the newest message sent to the bot",    &tg_learn_cmd },
        { "tg-test",  "Queue a test message: tg-test [text]",                         &tg_test_cmd  },
        { "tg-status", "Telegram settings, network and waiting messages",             &tg_status_cmd },
    };
    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        esp_console_cmd_t c = {};
        c.command = cmds[i].command;
        c.help = cmds[i].help;
        c.func = cmds[i].func;
        esp_console_cmd_register(&c);
    }
{{#if cfg.trigger_param}}
    app_driver_register_solution("{{prefix_lc}}_telegram", tg_trigger_cb, NULL);
{{/if}}
    if (s_tg_token[0] == 0) ESP_LOGW(TAG, "telegram: no bot token - run: tg-token <token>");
    else if (!s_tg_have_chat) ESP_LOGW(TAG, "telegram: no chat - send the bot a message, then run: tg-learn");
}
