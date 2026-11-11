#if CONFIG_SOC_WIFI_SUPPORTED || CONFIG_SOC_WIRELESS_HOST_SUPPORTED
static bool s_{{prefix_lc}}_owned = false;
static volatile bool s_{{prefix_lc}}_up = false;
static char s_{{prefix_lc}}_ssid[33] = {0};
static char s_{{prefix_lc}}_pass[65] = {0};
static uint32_t s_{{prefix_lc}}_backoff_s = 1;
static esp_timer_handle_t s_{{prefix_lc}}_retry = NULL;

static void {{prefix_lc}}_apply_config(void)
{
    wifi_config_t wc = {};
    strlcpy((char *)wc.sta.ssid, s_{{prefix_lc}}_ssid, sizeof(wc.sta.ssid));
    strlcpy((char *)wc.sta.password, s_{{prefix_lc}}_pass, sizeof(wc.sta.password));
    esp_wifi_set_config(WIFI_IF_STA, &wc);
}

static void {{prefix_lc}}_retry_cb(void *arg)
{
    (void)arg;
    if (s_{{prefix_lc}}_ssid[0] != 0) esp_wifi_connect();
}

static void {{prefix_lc}}_event_cb(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *d = (const wifi_event_sta_disconnected_t *)data;
        s_{{prefix_lc}}_up = false;
        if (!s_{{prefix_lc}}_owned || s_{{prefix_lc}}_ssid[0] == 0) return;
        ESP_LOGW(TAG, "wifi: disconnected (reason %d) - retrying in %u s",
                 d ? (int)d->reason : -1, (unsigned)s_{{prefix_lc}}_backoff_s);
        esp_timer_stop(s_{{prefix_lc}}_retry);
        esp_timer_start_once(s_{{prefix_lc}}_retry, (uint64_t)s_{{prefix_lc}}_backoff_s * 1000000ULL);
        s_{{prefix_lc}}_backoff_s *= 2;
        if (s_{{prefix_lc}}_backoff_s > {{prefix}}_WIFI_MAX_BACKOFF_S) s_{{prefix_lc}}_backoff_s = {{prefix}}_WIFI_MAX_BACKOFF_S;
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *e = (const ip_event_got_ip_t *)data;
        s_{{prefix_lc}}_up = true;
        s_{{prefix_lc}}_backoff_s = 1;
        if (e) ESP_LOGI(TAG, "wifi: connected to '%s', address " IPSTR, s_{{prefix_lc}}_ssid, IP2STR(&e->ip_info.ip));
    }
}

/* Stores the network and, when this block owns the radio, joins it. */
static int {{prefix_lc}}_save(const char *ssid, const char *pass)
{
    nvs_handle_t h;
    if (nvs_open("zc_wifi", NVS_READWRITE, &h) != ESP_OK) {
        printf("could not open storage - not saved\n");
        return 1;
    }
    esp_err_t err = nvs_set_str(h, "ssid", ssid);
    if (err == ESP_OK) err = nvs_set_str(h, "pass", pass);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if (err != ESP_OK) {
        printf("not saved: %s\n", esp_err_to_name(err));
        return 1;
    }
    strlcpy(s_{{prefix_lc}}_ssid, ssid, sizeof(s_{{prefix_lc}}_ssid));
    strlcpy(s_{{prefix_lc}}_pass, pass, sizeof(s_{{prefix_lc}}_pass));
    if (!s_{{prefix_lc}}_owned) {
        printf("saved, but another part of this firmware owns Wi-Fi - use its setup command\n");
        return 0;
    }
    s_{{prefix_lc}}_backoff_s = 1;
    esp_timer_stop(s_{{prefix_lc}}_retry);
    {{prefix_lc}}_apply_config();
    esp_wifi_disconnect();
    esp_wifi_connect();
    printf("saved - connecting to '%s'\n", s_{{prefix_lc}}_ssid);
    return 0;
}

static int {{prefix_lc}}_set_cmd(int argc, char **argv)
{
    if (argc < 2 || argc > 3) {
        printf("usage: wifi-set <ssid> [password]   (quote a name with spaces), or wifi-setup to be asked\n");
        return 1;
    }
    if (strlen(argv[1]) > 32 || (argc == 3 && strlen(argv[2]) > 64)) {
        printf("network name is at most 32 bytes, password at most 64\n");
        return 1;
    }
    return {{prefix_lc}}_save(argv[1], argc == 3 ? argv[2] : "");
}

/* wifi-setup: the guided version of wifi-set, one question at a time. */
#define {{prefix}}_WIFI_ANSWER_MS 120000  /* per question: a person may go and find the password */

static bool {{prefix_lc}}_ask(const char *prompt, char *buf, size_t len, bool secret)
{
    for (;;) {
        esp_err_t err = app_console_read_line(prompt, buf, len, secret, {{prefix}}_WIFI_ANSWER_MS);
        if (err == ESP_OK) return true;
        if (err == ESP_ERR_INVALID_SIZE) {
            printf("  too long - at most %u characters\n", (unsigned)(len - 1));
            continue;
        }
        printf("wifi-setup: no answer - stopped, nothing changed. Run wifi-setup again to finish.\n");
        return false;
    }
}

static int {{prefix_lc}}_setup_cmd(int argc, char **argv)
{
    (void)argc; (void)argv;
    char ssid[sizeof(s_{{prefix_lc}}_ssid)], pass[sizeof(s_{{prefix_lc}}_pass)], prompt[80];
    printf("wifi-setup - answer each question; Enter keeps the value in [brackets].\n");
    snprintf(prompt, sizeof(prompt), "Wi-Fi network (SSID) [%s]: ", s_{{prefix_lc}}_ssid);
    if (!{{prefix_lc}}_ask(prompt, ssid, sizeof(ssid), false)) return 1;
    bool keep_ssid = ssid[0] == 0;
    if (keep_ssid && s_{{prefix_lc}}_ssid[0] == 0) {
        printf("wifi-setup: no network given - nothing changed\n");
        return 1;
    }
    if (keep_ssid) strlcpy(ssid, s_{{prefix_lc}}_ssid, sizeof(ssid));
    /* Enter keeps the stored password for the same network; "-" means an open network. */
    if (!{{prefix_lc}}_ask(keep_ssid ? "Wi-Fi password (Enter keeps, - for an open network): "
                                     : "Wi-Fi password (- for an open network): ",
                           pass, sizeof(pass), true)) return 1;
    if (strcmp(pass, "-") == 0) pass[0] = 0;
    else if (pass[0] == 0 && keep_ssid) strlcpy(pass, s_{{prefix_lc}}_pass, sizeof(pass));
    return {{prefix_lc}}_save(ssid, pass);
}

static int {{prefix_lc}}_status_cmd(int argc, char **argv)
{
    (void)argc; (void)argv;
    printf("network  : %s\n", s_{{prefix_lc}}_ssid[0] ? s_{{prefix_lc}}_ssid : "(not set - run wifi-setup)");
    printf("owner    : %s\n", s_{{prefix_lc}}_owned ? "wifi_station" : "another part of the firmware");
    printf("connected: %s\n", s_{{prefix_lc}}_up ? "yes" : "no");
    esp_netif_t *sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t ip = {};
    if (sta != NULL && esp_netif_get_ip_info(sta, &ip) == ESP_OK && ip.ip.addr != 0) {
        printf("address  : " IPSTR "\n", IP2STR(&ip.ip));
    }
    wifi_ap_record_t ap = {};
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) printf("signal   : %d dBm\n", (int)ap.rssi);
    return 0;
}
#endif /* Wi-Fi, native or hosted */
