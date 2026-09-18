/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * AWS IoT Core connectivity: configuration, the network, and init.
 *
 * The session itself lives in zc_aws_agent.cpp and the onboarding commands in
 * zc_aws_console.cpp.
 */

#include "zc_aws_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <esp_event.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs.h>

const char *ZC_AWS_TAG = "app_aws_iot";

ESP_EVENT_DEFINE_BASE(ZC_AWS_EVENT);

char     g_zc_aws_ssid[33]      = {0};
char     g_zc_aws_pass[65]      = {0};
char     g_zc_aws_endpoint[129] = {0};
char     g_zc_aws_thing[65]     = {0};
int32_t  g_zc_aws_port          = 8883;
char    *g_zc_aws_cert = NULL;   size_t g_zc_aws_cert_len = 0;
char    *g_zc_aws_key = NULL;    size_t g_zc_aws_key_len = 0;
char    *g_zc_aws_rootca = NULL; size_t g_zc_aws_rootca_len = 0;

volatile bool g_zc_aws_net_up     = false;
volatile bool g_zc_aws_connected  = false;
volatile bool g_zc_aws_wifi_owned = false;

/* Amazon Root CA 1, compiled in. Not the IDF certificate bundle: that is ~64 KB
 * of flash for a trust store this device needs exactly one entry of, and AWS
 * documents this CA by name. `aws-rootca` overrides it from NVS for a private
 * endpoint. The length passed to mbedTLS must COUNT the NUL. */
const char ZC_AWS_ROOT_CA[] =
"-----BEGIN CERTIFICATE-----\n"
"MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF\n"
"ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6\n"
"b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL\n"
"MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv\n"
"b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj\n"
"ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM\n"
"9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw\n"
"IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6\n"
"VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L\n"
"93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm\n"
"jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC\n"
"AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA\n"
"A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI\n"
"U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs\n"
"N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv\n"
"o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU\n"
"5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy\n"
"rqXRfboQnoZsG4q5WTP468SQvvG5\n"
"-----END CERTIFICATE-----\n";

size_t zc_aws_root_ca_size(void)
{
    return sizeof(ZC_AWS_ROOT_CA);
}

bool zc_aws_configured(void)
{
    return g_zc_aws_endpoint[0] != 0 && g_zc_aws_thing[0] != 0 &&
           g_zc_aws_cert != NULL && g_zc_aws_key != NULL;
}

const char *app_aws_iot_thing_name(void)
{
    return g_zc_aws_thing;
}

bool app_aws_iot_connected(void)
{
    return g_zc_aws_connected;
}

void zc_aws_post_event(int32_t event_id)
{
    (void)esp_event_post(ZC_AWS_EVENT, event_id, NULL, 0, 0);
}

/* ── configuration in NVS ────────────────────────────────────────────── */

static char *nvs_load_pem(nvs_handle_t h, const char *key, size_t *out_len)
{
    size_t n = 0;
    if (nvs_get_blob(h, key, NULL, &n) != ESP_OK || n == 0) return NULL;
    char *b = (char *)malloc(n);
    if (b == NULL) return NULL;
    if (nvs_get_blob(h, key, b, &n) != ESP_OK) {
        free(b);
        return NULL;
    }
    *out_len = n;
    return b;
}

void zc_aws_load_config(void)
{
    nvs_handle_t h;
    if (nvs_open("zc_aws", NVS_READONLY, &h) != ESP_OK) return;
    size_t n;
    n = sizeof(g_zc_aws_ssid);     nvs_get_str(h, "ssid", g_zc_aws_ssid, &n);
    n = sizeof(g_zc_aws_pass);     nvs_get_str(h, "pass", g_zc_aws_pass, &n);
    n = sizeof(g_zc_aws_endpoint); nvs_get_str(h, "endpoint", g_zc_aws_endpoint, &n);
    n = sizeof(g_zc_aws_thing);    nvs_get_str(h, "thing", g_zc_aws_thing, &n);
    nvs_get_i32(h, "port", &g_zc_aws_port);
    free(g_zc_aws_cert);   g_zc_aws_cert   = nvs_load_pem(h, "cert", &g_zc_aws_cert_len);
    free(g_zc_aws_key);    g_zc_aws_key    = nvs_load_pem(h, "key", &g_zc_aws_key_len);
    free(g_zc_aws_rootca); g_zc_aws_rootca = nvs_load_pem(h, "rootca", &g_zc_aws_rootca_len);
    nvs_close(h);
}

/* ── the network ─────────────────────────────────────────────────────── */

static void net_event_handler(void *arg, esp_event_base_t base,
                              int32_t event_id, void *data)
{
    (void)arg;
    (void)data;
    if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        g_zc_aws_net_up = false;
        if (g_zc_aws_wifi_owned) esp_wifi_connect();
    } else if (base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        g_zc_aws_net_up = true;
        ESP_LOGI(ZC_AWS_TAG, "network up");
    }
}

void zc_aws_wifi_reconnect(void)
{
    if (!g_zc_aws_wifi_owned || g_zc_aws_ssid[0] == '\0') return;
    wifi_config_t wc = {};
    strlcpy((char *)wc.sta.ssid, g_zc_aws_ssid, sizeof(wc.sta.ssid));
    strlcpy((char *)wc.sta.password, g_zc_aws_pass, sizeof(wc.sta.password));
    esp_wifi_set_config(WIFI_IF_STA, &wc);
    esp_wifi_disconnect();
    esp_wifi_connect();
}

/**
 * Decide at RUNTIME whether this block owns Wi-Fi.
 *
 * Every other radio framework makes this decision at CODEGEN time, through a
 * `sharedNet` flag the generator derives from a fixed precedence chain. This
 * block emits no generated code, so it asks the system instead: if a station
 * netif already exists, or esp_wifi is already initialised, somebody else owns
 * the radio and we only watch for an address.
 *
 * It runs from the session task after a grace period rather than from init(),
 * because the session task is priority 5 and app_main is priority 1 — starting
 * it during init() would let it preempt app_main and answer the question before
 * a framework listed after us in `frameworks:` has had a chance to init. Two
 * seconds is invisible against a TLS handshake and covers either ordering.
 */
static void wifi_claim_or_defer(void)
{
    vTaskDelay(pdMS_TO_TICKS(2000));

    /* esp_netif_init() and the default event loop are already up — init() does
     * them synchronously, because behaviour slots register event handlers long
     * before this task runs. Only the RADIO decision is deferred. */
    wifi_mode_t mode;
    if (esp_wifi_get_mode(&mode) != ESP_ERR_WIFI_NOT_INIT) {
        g_zc_aws_wifi_owned = false;
        ESP_LOGI(ZC_AWS_TAG, "another framework owns Wi-Fi — waiting for an address");
        esp_netif_t *sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        esp_netif_ip_info_t ip = {};
        if (sta != NULL && esp_netif_get_ip_info(sta, &ip) == ESP_OK && ip.ip.addr != 0) {
            /* Already connected — the GOT_IP we would have waited for is past. */
            g_zc_aws_net_up = true;
        }
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, net_event_handler, NULL);
        return;
    }

    if (esp_netif_get_handle_from_ifkey("WIFI_STA_DEF") == NULL) {
        esp_netif_create_default_wifi_sta();
    }
    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&wcfg) != ESP_OK) {
        ESP_LOGE(ZC_AWS_TAG, "esp_wifi_init failed");
        return;
    }
    g_zc_aws_wifi_owned = true;

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, net_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, net_event_handler, NULL);

    esp_wifi_set_mode(WIFI_MODE_STA);
    if (g_zc_aws_ssid[0] != '\0') {
        wifi_config_t wc = {};
        strlcpy((char *)wc.sta.ssid, g_zc_aws_ssid, sizeof(wc.sta.ssid));
        strlcpy((char *)wc.sta.password, g_zc_aws_pass, sizeof(wc.sta.password));
        esp_wifi_set_config(WIFI_IF_STA, &wc);
    } else {
        ESP_LOGW(ZC_AWS_TAG, "no Wi-Fi credentials — run: aws-wifi <ssid> <password>");
    }
    esp_wifi_start();
    if (g_zc_aws_ssid[0] != '\0') esp_wifi_connect();
}

/* Runs once, on the session task, before the connect loop. Declared in the
 * internal header so zc_aws_agent.cpp can call it. */
extern "C" void zc_aws_net_start(void)
{
    wifi_claim_or_defer();
}

/* ── init ────────────────────────────────────────────────────────────── */

esp_err_t app_aws_iot_init(void)
{
    /* The netif layer and the default event loop come up HERE, synchronously,
     * even though the Wi-Fi ownership decision is deferred to the session task.
     * They are not the radio: creating them claims nothing, and every behaviour
     * that registers a ZC_AWS_EVENT handler from its logic_init slot needs the
     * loop to exist by the time app_main reaches it — which is before the
     * session task has run at all. Doing these two in the deferred path meant
     * esp_event_handler_register() failed silently and connect events reached
     * nobody. Both are idempotent; ESP_ERR_INVALID_STATE means somebody else
     * got there first, which is fine. */
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(ZC_AWS_TAG, "esp_netif_init: %s", esp_err_to_name(err));
        return err;
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(ZC_AWS_TAG, "esp_event_loop_create_default: %s", esp_err_to_name(err));
        return err;
    }

    zc_aws_subs_init();
    zc_aws_console_register();
    zc_aws_load_config();

    /* Default the thing name to the station MAC so a device that has only had
     * its certificate installed still has a stable identity. AWS matches the
     * client id against the certificate's policy, so this is a default, not a
     * claim — aws-thing overrides it. */
    if (g_zc_aws_thing[0] == 0) {
        uint8_t mac[6] = {0};
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        snprintf(g_zc_aws_thing, sizeof(g_zc_aws_thing),
                 "zerocode-%02x%02x%02x%02x%02x%02x",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    if (!zc_aws_configured()) {
        /* Unconfigured is a valid state — never block boot. This is also what
         * lets CI build a product for this framework with no AWS account. */
        ESP_LOGW(ZC_AWS_TAG,
                 "not configured — run: aws-endpoint <host>, aws-thing <name>,"
                 " then aws-cert / aws-key and paste each PEM");
    }

    err = zc_aws_agent_start();
    if (err != ESP_OK) return err;

    ESP_LOGI(ZC_AWS_TAG, "AWS IoT connectivity ready (thing \"%s\")", g_zc_aws_thing);
    return ESP_OK;
}
