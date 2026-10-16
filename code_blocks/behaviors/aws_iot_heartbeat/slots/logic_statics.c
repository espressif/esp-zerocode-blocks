static char s_{{prefix_lc}}_hb_topic[128] = {0};
static char s_{{prefix_lc}}_cmd_topic[128] = {0};

/* Runs on the AWS session task. Keep it short — it is the same task that drives
 * the MQTT command loop. */
static void {{prefix_lc}}_on_cmd(const char *topic, uint16_t topic_len,
                                 const void *payload, size_t len, void *ctx)
{
    (void)ctx;
    /* Neither the topic nor the payload is NUL-terminated, and the payload is
     * only valid for this call. json_parse_start takes a length, so the text
     * is read in place — no copy, no terminator needed. */
    ESP_LOGI(TAG, "{{prefix_lc}}: %.*s -> %.*s",
             (int)topic_len, topic, (int)len, (const char *)payload);

    /* A command is a JSON object such as {"action":"ping","value":3}. Each
     * accessor answers OS_SUCCESS only when the member exists AND has the
     * requested type; a string that does not fit `action` fails rather than
     * truncating. Anything else is logged as-is above and otherwise ignored. */
    jparse_ctx_t jctx;
    if (json_parse_start(&jctx, (const char *)payload, (int)len) != OS_SUCCESS) {
        ESP_LOGW(TAG, "{{prefix_lc}}: command is not a JSON document");
        return;
    }
    char action[32];
    int value = 0;
    if (json_obj_get_string(&jctx, "action", action, sizeof(action)) == OS_SUCCESS) {
        bool has_value = json_obj_get_int(&jctx, "value", &value) == OS_SUCCESS;
        ESP_LOGI(TAG, "{{prefix_lc}}: action '%s'%s%d", action,
                 has_value ? " value " : "", has_value ? value : 0);
    }
    json_parse_end(&jctx);
}

/* Topics are built HERE, on every connect, from the thing name that connect
 * uses: two snprintf calls, and never a topic for a name the session is not
 * using. */
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
{{#if cfg.presence}}
    /* The other half of the last will: retained "online" on every connect, so
     * the status topic always says the truth — "offline" from the broker when
     * the device vanished, "online" from the device once it is back. */
    char status[128];
    snprintf(status, sizeof(status), "%s/%s/status", {{prefix}}_TOPIC_PREFIX, thing);
    esp_err_t err = app_aws_iot_publish_retained(status, "online", 6, 1, pdMS_TO_TICKS(1000));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "{{prefix_lc}}: online status not sent: %s", esp_err_to_name(err));
    }
{{/if}}
}

static void {{prefix_lc}}_beat(void *arg)
{
    (void)arg;
    if (!app_aws_iot_connected() || s_{{prefix_lc}}_hb_topic[0] == '\0') {
        ESP_LOGD(TAG, "{{prefix_lc}}: session down — skipping this beat");
        return;
    }
    /* The payload is built with json_generator into a buffer this function
     * owns: start, one object, members, end. Every string it writes is escaped
     * and UTF-8 checked, so a thing name with a quote in it cannot break the
     * document the way an snprintf format would. json_gen_str_end() returns the
     * length INCLUDING the terminating NUL, or a negative JSON_GEN_ERR_* when
     * something did not fit — then the buffer is unspecified and nothing is
     * sent. */
    char body[160];
    json_gen_str_t gen;
    json_gen_str_start(&gen, body, sizeof(body), NULL, NULL);
    json_gen_start_object(&gen);
    json_gen_obj_set_int64(&gen, "uptime_s", esp_timer_get_time() / 1000000);
    json_gen_obj_set_string(&gen, "thing", app_aws_iot_thing_name());
    json_gen_end_object(&gen);
    int n = json_gen_str_end(&gen);
    if (n <= 1) {
        ESP_LOGW(TAG, "{{prefix_lc}}: heartbeat payload did not fit (%d)", n);
        return;
    }

    /* Called from the esp_timer task, not the session task — which is the whole
     * point of the agent underneath: any task may publish. */
    esp_err_t err = app_aws_iot_publish(s_{{prefix_lc}}_hb_topic, body, (size_t)(n - 1),
                                        0, pdMS_TO_TICKS(1000));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "{{prefix_lc}}: heartbeat not sent: %s", esp_err_to_name(err));
    }
}
