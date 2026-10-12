/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * The Wi-Fi setter (app_aws_iot.h): validate, persist to NVS (namespace
 * "zc_aws"), update the live config. aws-wifi and aws-setup are thin wrappers
 * over it, so a product never needs to know the NVS layout. The AWS identity
 * has no setter: it is the device data written at flash time.
 */

#include "zc_aws_internal.h"

#include <string.h>

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <nvs.h>

static const char *NVS_NS = "zc_aws";

esp_err_t app_aws_iot_set_wifi(const char *ssid, const char *password)
{
    if (ssid == NULL || ssid[0] == '\0') return ESP_ERR_INVALID_ARG;
    if (password == NULL) password = "";
    if (strlen(ssid) >= sizeof(g_zc_aws_ssid) || strlen(password) >= sizeof(g_zc_aws_pass)) {
        return ESP_ERR_INVALID_SIZE;
    }
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_str(h, "ssid", ssid);
    if (err == ESP_OK) err = nvs_set_str(h, "pass", password);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if (err != ESP_OK) return err;
    zc_aws_cfg_lock();
    strlcpy(g_zc_aws_ssid, ssid, sizeof(g_zc_aws_ssid));
    strlcpy(g_zc_aws_pass, password, sizeof(g_zc_aws_pass));
    zc_aws_cfg_unlock();
    if (g_zc_aws_wifi_owned) zc_aws_wifi_reconnect();
    zc_aws_config_changed();
    return ESP_OK;
}

bool app_aws_iot_configured(void)
{
    return zc_aws_configured();
}

bool app_aws_iot_owns_wifi(void)
{
    return g_zc_aws_wifi_owned;
}

/* ── last will ───────────────────────────────────────────────────────── */

/* Copies, so the caller's strings need not outlive the call. Set from an app
 * task, read by the session task at connect: a short critical section around
 * the copy is enough for strings this size. */
static portMUX_TYPE s_will_lock = portMUX_INITIALIZER_UNLOCKED;
static char s_will_topic[128];
static char s_will_payload[256];
static uint8_t s_will_qos;
static bool s_will_retain;

esp_err_t app_aws_iot_set_will(const char *topic, const char *payload, uint8_t qos, bool retain)
{
    if (topic == NULL) {
        portENTER_CRITICAL(&s_will_lock);
        s_will_topic[0] = '\0';
        portEXIT_CRITICAL(&s_will_lock);
        return ESP_OK;
    }
    if (topic[0] == '\0' || payload == NULL || qos > 1) return ESP_ERR_INVALID_ARG;
    if (strlen(topic) >= sizeof(s_will_topic) || strlen(payload) >= sizeof(s_will_payload)) {
        return ESP_ERR_INVALID_SIZE;
    }
    portENTER_CRITICAL(&s_will_lock);
    strlcpy(s_will_topic, topic, sizeof(s_will_topic));
    strlcpy(s_will_payload, payload, sizeof(s_will_payload));
    s_will_qos = qos;
    s_will_retain = retain;
    portEXIT_CRITICAL(&s_will_lock);
    return ESP_OK;
}

bool zc_aws_will_for_connect(MQTTPublishInfo_t *out, const char *thing,
                             char *topic_buf, size_t topic_cap, char *payload_buf, size_t payload_cap)
{
    char tmpl[sizeof(s_will_topic)];
    portENTER_CRITICAL(&s_will_lock);
    memcpy(tmpl, s_will_topic, sizeof(tmpl));
    size_t plen = strnlen(s_will_payload, sizeof(s_will_payload));
    bool payload_fits = plen < payload_cap;
    if (payload_fits) { memcpy(payload_buf, s_will_payload, plen); payload_buf[plen] = '\0'; }
    bool retain = s_will_retain;
    uint8_t qos = s_will_qos;
    portEXIT_CRITICAL(&s_will_lock);
    if (tmpl[0] == '\0') return false;
    if (!payload_fits) {
        ESP_LOGW(ZC_AWS_TAG, "last will payload over %u bytes — connecting without a will", (unsigned)payload_cap - 1);
        return false;
    }

    /* "{thing}" → the thing name this connect uses. */
    size_t n = 0;
    for (const char *p = tmpl; *p != '\0';) {
        const char *ins = NULL;
        size_t adv = 1;
        if (strncmp(p, "{thing}", 7) == 0) { ins = thing; adv = 7; }
        size_t len = ins ? strlen(ins) : 1;
        if (n + len >= topic_cap) {
            ESP_LOGW(ZC_AWS_TAG, "last will topic over %u bytes — connecting without a will", (unsigned)topic_cap - 1);
            return false;
        }
        memcpy(topic_buf + n, ins ? ins : p, len);
        n += len;
        p += adv;
    }
    topic_buf[n] = '\0';

    *out = MQTTPublishInfo_t{};
    out->qos = qos ? MQTTQoS1 : MQTTQoS0;
    out->retain = retain;
    out->pTopicName = topic_buf;
    out->topicNameLength = (uint16_t)n;
    out->pPayload = payload_buf;
    out->payloadLength = plen;
    return true;
}
