static char s_{{prefix_lc}}_hb_topic[128] = {0};
static char s_{{prefix_lc}}_cmd_topic[128] = {0};

/* Runs on the AWS session task. Keep it short — it is the same task that drives
 * the MQTT command loop. */
static void {{prefix_lc}}_on_cmd(const char *topic, uint16_t topic_len,
                                 const void *payload, size_t len, void *ctx)
{
    (void)ctx;
    /* Neither the topic nor the payload is NUL-terminated, and the payload is
     * only valid for this call. */
    ESP_LOGI(TAG, "{{prefix_lc}}: %.*s -> %.*s",
             (int)topic_len, topic, (int)len, (const char *)payload);
}

/* Topics are built HERE, on connect, and not in logic_init.
 *
 * app_aws_iot_thing_name() returns a MAC-derived default until the thing name
 * is configured, and onboarding happens over the console long after behaviour
 * init has run. Caching the topics at init means subscribing to the default
 * name and never noticing — the device connects, the console shows a healthy
 * session, and nothing is ever received. Rebuilding on every connect costs two
 * snprintf calls and cannot go stale. */
static void {{prefix_lc}}_on_connected(void *arg, esp_event_base_t base,
                                       int32_t id, void *data)
{
    (void)arg; (void)base; (void)id; (void)data;

    char hb[128], cmd[128];
    const char *thing = app_aws_iot_thing_name();
    snprintf(hb, sizeof(hb), "%s/%s/heartbeat", {{prefix}}_TOPIC_PREFIX, thing);
    snprintf(cmd, sizeof(cmd), "%s/%s/cmd", {{prefix}}_TOPIC_PREFIX, thing);

    bool changed = strcmp(cmd, s_{{prefix_lc}}_cmd_topic) != 0;
    strlcpy(s_{{prefix_lc}}_hb_topic, hb, sizeof(s_{{prefix_lc}}_hb_topic));
    strlcpy(s_{{prefix_lc}}_cmd_topic, cmd, sizeof(s_{{prefix_lc}}_cmd_topic));

    /* Only on a change: the framework replays existing registrations itself, so
     * re-sending an unchanged filter on every reconnect is pure duplicate. */
    if (changed) {
        esp_err_t err = app_aws_iot_subscribe(s_{{prefix_lc}}_cmd_topic, 1,
                                              {{prefix_lc}}_on_cmd, NULL);
        ESP_LOGI(TAG, "{{prefix_lc}}: subscribe %s: %s",
                 s_{{prefix_lc}}_cmd_topic, esp_err_to_name(err));
    }
    ESP_LOGI(TAG, "{{prefix_lc}}: heartbeats to %s", s_{{prefix_lc}}_hb_topic);
}

static void {{prefix_lc}}_beat(void *arg)
{
    (void)arg;
    if (!app_aws_iot_connected() || s_{{prefix_lc}}_hb_topic[0] == '\0') {
        ESP_LOGD(TAG, "{{prefix_lc}}: session down — skipping this beat");
        return;
    }
    char body[96];
    int n = snprintf(body, sizeof(body), "{\"uptime_s\":%lld}",
                     (long long)(esp_timer_get_time() / 1000000));
    if (n < 0 || (size_t)n >= sizeof(body)) return;

    /* Called from the esp_timer task, not the session task — which is the whole
     * point of the agent underneath: any task may publish. */
    esp_err_t err = app_aws_iot_publish(s_{{prefix_lc}}_hb_topic, body, (size_t)n,
                                        0, pdMS_TO_TICKS(1000));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "{{prefix_lc}}: heartbeat not sent: %s", esp_err_to_name(err));
    }
}
