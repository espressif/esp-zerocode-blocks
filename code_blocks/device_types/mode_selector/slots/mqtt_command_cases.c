if (tlen == (int)strlen(s_{{prefix_lc}}_cmd_topic) &&
    strncmp(topic, s_{{prefix_lc}}_cmd_topic, tlen) == 0) {
    /* Payload is an option name, "Mode <n>" with n in 1..mode_count. */
    char pbuf[16];
    int plen = dlen < 15 ? dlen : 15;
    memcpy(pbuf, data, plen); pbuf[plen] = 0;
    if (strncmp(pbuf, "Mode ", 5) == 0) {
        int n = atoi(pbuf + 5);
        if (n >= 1 && n <= ZC_{{prefix_lc}}_MODE_COUNT) {
            app_driver_param_val_t v = { .u8 = (uint8_t)(n - 1) };
            app_driver_set_param({{cfg.mode_param}}, v, s_handle);
            {{prefix_lc}}_publish_mode((uint8_t)(n - 1));
        }
    }
    return;
}
