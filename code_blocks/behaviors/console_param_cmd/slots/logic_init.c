{
    /* Zero-init + assign: newer IDF adds fields to esp_console_cmd_t and the
     * build runs with -Werror=missing-field-initializers. */
    esp_console_cmd_t {{prefix_lc}}_c = {};
    {{prefix_lc}}_c.command = {{prefix}}_CMD_NAME;
    {{prefix_lc}}_c.help = "Set {{cfg.target_param}} ({{cfg.value_kind}}, {{cfg.mode}})";
    {{prefix_lc}}_c.func = &{{prefix_lc}}_cmd;
    esp_console_cmd_register(&{{prefix_lc}}_c);
    ESP_LOGI(TAG, "{{prefix_lc}}: console cmd '%s' ({{cfg.value_kind}}) registered", {{prefix}}_CMD_NAME);
}
