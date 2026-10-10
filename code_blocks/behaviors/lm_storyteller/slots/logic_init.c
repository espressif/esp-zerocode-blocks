{
    esp_console_cmd_t {{prefix_lc}}_c = {};
    {{prefix_lc}}_c.command = "{{cfg.command_name}}";
    {{prefix_lc}}_c.help = "Generate a story on-device from an opening line";
    {{prefix_lc}}_c.hint = "<opening words...>";
    {{prefix_lc}}_c.func = &{{prefix_lc}}_cmd;
    esp_console_cmd_register(&{{prefix_lc}}_c);
}
