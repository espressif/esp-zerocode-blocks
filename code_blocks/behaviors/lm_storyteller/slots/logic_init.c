{
    const esp_console_cmd_t {{prefix_lc}}_c = {
        .command = "{{cfg.command_name}}",
        .help = "Generate a story on-device from an opening line",
        .hint = "<opening words...>",
        .func = &{{prefix_lc}}_cmd,
    };
    esp_console_cmd_register(&{{prefix_lc}}_c);
}
