{
    /* Zero-init + assign: newer IDF adds fields to esp_console_cmd_t and the
     * build runs with -Werror=missing-field-initializers. */
    esp_console_cmd_t {{prefix_lc}}_c = {};
    {{prefix_lc}}_c.command = "version";
    {{prefix_lc}}_c.help = "Firmware + IDF version";
    {{prefix_lc}}_c.func = &{{prefix_lc}}_cmd;
    esp_console_cmd_register(&{{prefix_lc}}_c);
}
