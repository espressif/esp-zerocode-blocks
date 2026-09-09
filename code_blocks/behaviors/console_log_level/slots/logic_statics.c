static int {{prefix_lc}}_cmd(int argc, char **argv)
{
    if (argc != 3) {
        printf("usage: log <tag|*> <NONE|ERROR|WARN|INFO|DEBUG|VERBOSE>\n");
        return 1;
    }
    esp_log_level_t lvl;
    if      (!strcasecmp(argv[2], "NONE"))    lvl = ESP_LOG_NONE;
    else if (!strcasecmp(argv[2], "ERROR"))   lvl = ESP_LOG_ERROR;
    else if (!strcasecmp(argv[2], "WARN"))    lvl = ESP_LOG_WARN;
    else if (!strcasecmp(argv[2], "INFO"))    lvl = ESP_LOG_INFO;
    else if (!strcasecmp(argv[2], "DEBUG"))   lvl = ESP_LOG_DEBUG;
    else if (!strcasecmp(argv[2], "VERBOSE")) lvl = ESP_LOG_VERBOSE;
    else { printf("unknown level: %s\n", argv[2]); return 1; }
    esp_log_level_set(argv[1], lvl);
    printf("log level for '%s' set to %s\n", argv[1], argv[2]);
    return 0;
}
