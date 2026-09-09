static int {{prefix_lc}}_cmd(int argc, char **argv)
{
    const esp_app_desc_t *desc = esp_app_get_description();
    printf("firmware:    %s\n", desc ? desc->project_name : "(unknown)");
    printf("version:     %s\n", desc ? desc->version : "(unknown)");
    printf("build date:  %s %s\n", desc ? desc->date : "?", desc ? desc->time : "?");
    printf("IDF version: %s\n", esp_get_idf_version());
    return 0;
}
