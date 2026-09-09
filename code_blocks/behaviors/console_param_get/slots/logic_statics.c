static int {{prefix_lc}}_cmd(int argc, char **argv)
{
    app_driver_param_val_t v = {};
    esp_err_t err = app_driver_get_param({{cfg.target_param}}, &v);
    if (err != ESP_OK) {
        printf("error: %s\n", esp_err_to_name(err));
        return 1;
    }
    /* value_kind={{cfg.value_kind}} → pick the union member */
    if      (!strcmp("{{cfg.value_kind}}", "bool")) printf("{{cfg.target_param}} = %s\n", v.b ? "true" : "false");
    else if (!strcmp("{{cfg.value_kind}}", "u8"))   printf("{{cfg.target_param}} = %u\n", (unsigned)v.u8);
    else if (!strcmp("{{cfg.value_kind}}", "u16"))  printf("{{cfg.target_param}} = %u\n", (unsigned)v.u16);
    else if (!strcmp("{{cfg.value_kind}}", "i16"))  printf("{{cfg.target_param}} = %d\n", (int)v.i16);
    else                                            printf("{{cfg.target_param}} = %" PRIu32 "\n", v.u32);
    return 0;
}
