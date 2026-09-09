{
    const esp_console_cmd_t {{prefix_lc}}_c = {
        .command = "{{cfg.command_name}}",
        .help = "Send a plain-language command to the on-device model",
        .hint = "<words...>",
        .func = &{{prefix_lc}}_cmd,
    };
    esp_console_cmd_register(&{{prefix_lc}}_c);
}
