/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * The configuration setters (app_aws_iot.h): validate, persist to NVS
 * (namespace "zc_aws"), update the live config. The aws-* console commands and
 * aws-setup are thin wrappers over these, so there is ONE implementation of the
 * storage and a product never needs to know the NVS layout.
 */

#include "zc_aws_internal.h"

#include <stdlib.h>
#include <string.h>

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <mbedtls/pk.h>
#include <mbedtls/platform_util.h>
#include <mbedtls/x509_crt.h>
#include <nvs.h>

static const char *NVS_NS = "zc_aws";

/* One string key, committed. */
static esp_err_t nvs_put_str(const char *key, const char *value)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_str(h, key, value);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

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

esp_err_t app_aws_iot_set_endpoint(const char *host, uint16_t port)
{
    if (host == NULL || host[0] == '\0') return ESP_ERR_INVALID_ARG;
    if (port == 0) port = 8883;
    if (port != 8883 && port != 443) return ESP_ERR_INVALID_ARG;
    if (strlen(host) >= sizeof(g_zc_aws_endpoint)) return ESP_ERR_INVALID_SIZE;
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_str(h, "endpoint", host);
    if (err == ESP_OK) err = nvs_set_i32(h, "port", port);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if (err != ESP_OK) return err;
    zc_aws_cfg_lock();
    strlcpy(g_zc_aws_endpoint, host, sizeof(g_zc_aws_endpoint));
    g_zc_aws_port = port;
    zc_aws_cfg_unlock();
    zc_aws_config_changed();
    return ESP_OK;
}

esp_err_t app_aws_iot_set_thing(const char *name)
{
    if (name == NULL || name[0] == '\0') return ESP_ERR_INVALID_ARG;
    if (strlen(name) >= sizeof(g_zc_aws_thing)) return ESP_ERR_INVALID_SIZE;
    esp_err_t err = nvs_put_str("thing", name);
    if (err != ESP_OK) return err;
    zc_aws_cfg_lock();
    strlcpy(g_zc_aws_thing, name, sizeof(g_zc_aws_thing));
    zc_aws_cfg_unlock();
    zc_aws_config_changed();
    return ESP_OK;
}

/* Does this text parse as what it claims to be? Cheap next to a failed TLS
 * handshake, and the only check that catches a truncated paste. */
bool zc_aws_pem_parses(const char *tag, const char *pem)
{
    /* PEM input to mbedTLS: the length INCLUDES the NUL. */
    size_t len = strlen(pem) + 1;
    int rc;
    if (strcmp(tag, "key") == 0) {
        mbedtls_pk_context pk;
        mbedtls_pk_init(&pk);
        rc = mbedtls_pk_parse_key(&pk, (const unsigned char *)pem, len, NULL, 0);
        mbedtls_pk_free(&pk);
    } else {
        mbedtls_x509_crt crt;
        mbedtls_x509_crt_init(&crt);
        rc = mbedtls_x509_crt_parse(&crt, (const unsigned char *)pem, len);
        mbedtls_x509_crt_free(&crt);
    }
    if (rc != 0) {
        /* Code only: CONFIG_MBEDTLS_ERROR_STRINGS is off in most builds, so
         * mbedtls_strerror would just print "UNKNOWN ERROR CODE". */
        ESP_LOGW(ZC_AWS_TAG, "%s: parse failed (-0x%04x)", tag, (unsigned)-rc);
    }
    return rc == 0;
}

static esp_err_t set_pem(const char *tag, const char *pem)
{
    if (pem == NULL || pem[0] == '\0') return ESP_ERR_INVALID_ARG;
    if (!zc_aws_pem_parses(tag, pem)) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_blob(h, tag, pem, strlen(pem) + 1);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if (err != ESP_OK) return err;
    /* Reloading frees and reallocates the PEM buffers a connect in progress
     * hands to the TLS handshake — so it waits for that connect to finish. */
    zc_aws_cfg_lock();
    zc_aws_load_config();
    zc_aws_cfg_unlock();
    zc_aws_config_changed();
    return ESP_OK;
}

esp_err_t app_aws_iot_set_cert(const char *pem) { return set_pem("cert", pem); }
esp_err_t app_aws_iot_set_key(const char *pem)  { return set_pem("key", pem); }

esp_err_t app_aws_iot_set_root_ca(const char *pem)
{
    if (pem != NULL) return set_pem("rootca", pem);
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    nvs_erase_key(h, "rootca");
    err = nvs_commit(h);
    nvs_close(h);
    zc_aws_cfg_lock();
    free(g_zc_aws_rootca); g_zc_aws_rootca = NULL; g_zc_aws_rootca_len = 0;
    zc_aws_cfg_unlock();
    zc_aws_config_changed();
    return err;
}

esp_err_t app_aws_iot_clear_credentials(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    nvs_erase_key(h, "cert");
    nvs_erase_key(h, "key");
    nvs_erase_key(h, "rootca");
    err = nvs_commit(h);
    nvs_close(h);
    zc_aws_cfg_lock();
    free(g_zc_aws_cert);   g_zc_aws_cert = NULL;   g_zc_aws_cert_len = 0;
    if (g_zc_aws_key != NULL) mbedtls_platform_zeroize(g_zc_aws_key, g_zc_aws_key_len);
    free(g_zc_aws_key);    g_zc_aws_key = NULL;    g_zc_aws_key_len = 0;
    free(g_zc_aws_rootca); g_zc_aws_rootca = NULL; g_zc_aws_rootca_len = 0;
    zc_aws_cfg_unlock();
    zc_aws_config_changed();
    return err;
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

    /* "{thing}" → the thing name this connect uses, which is the whole reason
     * the topic is a template: onboarding renames the thing after init. */
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
