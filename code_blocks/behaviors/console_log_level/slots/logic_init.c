{
    /* Zero-init + assign: newer IDF adds fields to esp_console_cmd_t and the
     * build runs with -Werror=missing-field-initializers. */
    esp_console_cmd_t {{prefix_lc}}_c = {};
    {{prefix_lc}}_c.command = "log";
    {{prefix_lc}}_c.help = "Set runtime log level for a tag";
    {{prefix_lc}}_c.hint = "<tag|*> <NONE|ERROR|WARN|INFO|DEBUG|VERBOSE>";
    {{prefix_lc}}_c.func = &{{prefix_lc}}_cmd;
    esp_console_cmd_register(&{{prefix_lc}}_c);
}
