/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * Onboarding over the serial console.
 *
 * An AWS IoT thing has no commissioning protocol the way Matter or RainMaker
 * do, so v1 is the console plus NVS (namespace "zc_aws"). The ZeroCode AI
 * browser drives exactly these commands when a user pastes an endpoint and
 * credentials.
 */

#include "zc_aws_internal.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_console.h>
#include <esp_log.h>
#include <mbedtls/pk.h>
#include <mbedtls/x509_crt.h>
#include <nvs.h>

#include "app_console.h"

/* Does this blob actually parse as what it claims to be? Cheap next to a failed
 * TLS handshake, and it is the only check that survives the truncation case —
 * see the comment at the call site. */
static bool cred_parses(const char *tag, const unsigned char *pem, size_t len)
{
    int rc;
    if (strcmp(tag, "key") == 0) {
        mbedtls_pk_context pk;
        mbedtls_pk_init(&pk);
        rc = mbedtls_pk_parse_key(&pk, pem, len, NULL, 0);
        mbedtls_pk_free(&pk);
    } else {
        mbedtls_x509_crt crt;
        mbedtls_x509_crt_init(&crt);
        rc = mbedtls_x509_crt_parse(&crt, pem, len);
        mbedtls_x509_crt_free(&crt);
    }
    if (rc != 0) {
        /* Code only: CONFIG_MBEDTLS_ERROR_STRINGS is off in most builds, so
         * mbedtls_strerror would just print "UNKNOWN ERROR CODE" over the top
         * of the actionable message the caller prints next. */
        printf("%s: parse failed (-0x%04x)\n", tag, (unsigned)-rc);
    }
    return rc == 0;
}

/* Store a validated PEM under `tag` in NVS and refresh the live config. */
static bool cred_store(const char *tag, const unsigned char *pem, size_t len_with_nul)
{
    nvs_handle_t h;
    if (nvs_open("zc_aws", NVS_READWRITE, &h) != ESP_OK) return false;
    esp_err_t err = nvs_set_blob(h, tag, pem, len_with_nul);
    nvs_commit(h);
    nvs_close(h);
    if (err != ESP_OK) return false;
    zc_aws_load_config();
    return true;
}

#define ZC_PASTE_MAX        8192    /* an RSA-4096 key PEM is ~3.2 KB */
#define ZC_PASTE_TOTAL_MS   60000
#define ZC_PASTE_IDLE_MS    8000

/* PASTE MODE: consume a PEM verbatim from the console.
 *
 * A PEM cannot be sent as a command argument — the REPL ends a command at the
 * newline, so a 20-line certificate arrives as 20 commands and 19 of them are
 * "Unrecognized command". Reading stdin directly from inside the handler works
 * because the REPL is blocked here until we return.
 *
 * This is the better path in every measurable way, which is why it is the
 * default. A PEM's own 64-character line wrapping sits far below the ~246
 * characters at which USB-Serial-JTAG silently truncates a line, so the
 * truncation that corrupts chunked base64 cannot arise; and base64-ing a PEM
 * encodes an already-base64 body a second time, costing a third more bytes.
 *
 * Nothing is echoed: linenoise is not running inside a command handler.
 */
static int cred_paste(const char *tag)
{
    printf("%s: paste the PEM now, including the BEGIN/END lines.\n"
           "     (ends by itself at the -----END line; %d s to start)\n",
           tag, ZC_PASTE_TOTAL_MS / 1000);
    fflush(stdout);

    char *buf = (char *)malloc(ZC_PASTE_MAX);
    if (buf == NULL) { printf("%s: out of memory\n", tag); return 1; }

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    size_t len = 0, line_start = 0;
    bool done = false, overflow = false, started = false;
    TickType_t start = xTaskGetTickCount(), last = start;
    char rx[128];

    while (!done) {
        /* Bulk reads, not fgetc per character: the console's receive buffer is
         * small and a sender that outruns this loop loses bytes silently. */
        int n = read(STDIN_FILENO, rx, sizeof(rx));
        if (n <= 0) {
            TickType_t now = xTaskGetTickCount();
            if ((now - start) * portTICK_PERIOD_MS > ZC_PASTE_TOTAL_MS) break;
            if (len > 0 && (now - last) * portTICK_PERIOD_MS > ZC_PASTE_IDLE_MS) break;
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }
        last = xTaskGetTickCount();
        for (int i = 0; i < n && !done; i++) {
            char c = rx[i];
            if (c == '\r') continue;             /* tolerate CRLF senders */
            if (!started) {
                /* Ignore anything before the armour: the newline that ended the
                 * command line arrives here, and a terminal may echo. Starting
                 * at -----BEGIN makes the paste independent of all of it. */
                if (c != '-') continue;
                started = true;
                line_start = len;
            }
            if (len + 2 >= ZC_PASTE_MAX) { overflow = true; done = true; break; }
            buf[len++] = c;
            if (c == '\n') {
                /* A line beginning "-----END" ends the paste — the PEM says
                 * where it stops, so no sentinel need be agreed with the sender. */
                if (len - line_start >= 8 && memcmp(buf + line_start, "-----END", 8) == 0) {
                    done = true;
                }
                line_start = len;
            }
        }
    }

    fcntl(STDIN_FILENO, F_SETFL, flags);
    buf[len] = '\0';

    if (overflow) {
        printf("%s: more than %d bytes pasted — aborted\n", tag, ZC_PASTE_MAX);
        free(buf);
        return 1;
    }
    if (!done) {
        printf("%s: no -----END line arrived (%u bytes read) — nothing stored\n",
               tag, (unsigned)len);
        free(buf);
        return 1;
    }
    if (!cred_parses(tag, (const unsigned char *)buf, len + 1)) {
        printf("%s: %u bytes pasted but they are not a valid %s\n",
               tag, (unsigned)len,
               (strcmp(tag, "key") == 0) ? "private key" : "certificate");
        free(buf);
        return 1;
    }
    bool ok = cred_store(tag, (const unsigned char *)buf, len + 1);
    printf(ok ? "%s saved (%u bytes, pasted)\n" : "%s: NVS write failed\n",
           tag, (unsigned)(len + 1));
    free(buf);
    return ok ? 0 : 1;
}

static int aws_cred_cmd(int argc, char **argv)
{
    /* One handler for aws-cert / aws-key / aws-rootca: esp_console passes the
     * command name as argv[0], and the NVS key is its tail. */
    const char *tag = argv[0] + 4;

    /* There is only one way in, and it takes no arguments. An earlier revision
     * also accepted the PEM as chunked base64, which is how a 1220-byte
     * certificate could arrive as 539 bytes of nonsense and be stored: the
     * transport truncates a long line without telling either end. Pasting the
     * PEM removes that failure by construction — its own 64-character line
     * wrapping is far below the limit — and it costs a third fewer bytes than
     * base64-ing a body that is already base64. */
    if (argc > 1) {
        printf("usage: %s   (no arguments — then paste the PEM, BEGIN/END lines "
               "included)\n", argv[0]);
        return 1;
    }
    return cred_paste(tag);
}

static int aws_endpoint_cmd(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage: aws-endpoint <host> [port]   (e.g. a1b2c3-ats.iot.eu-west-1.amazonaws.com)\n");
        return 1;
    }
    int32_t port = (argc > 2) ? (int32_t)strtol(argv[2], NULL, 10) : 8883;
    if (port != 8883 && port != 443) {
        printf("port must be 8883 (MQTT/TLS) or 443 (ALPN)\n");
        return 1;
    }
    nvs_handle_t h;
    if (nvs_open("zc_aws", NVS_READWRITE, &h) != ESP_OK) return 1;
    nvs_set_str(h, "endpoint", argv[1]);
    nvs_set_i32(h, "port", port);
    nvs_commit(h);
    nvs_close(h);
    strlcpy(g_zc_aws_endpoint, argv[1], sizeof(g_zc_aws_endpoint));
    g_zc_aws_port = port;
    printf("endpoint saved: %s:%d\n", g_zc_aws_endpoint, (int)g_zc_aws_port);
    return 0;
}

static int aws_thing_cmd(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage: aws-thing <name>\n");
        return 1;
    }
    nvs_handle_t h;
    if (nvs_open("zc_aws", NVS_READWRITE, &h) != ESP_OK) return 1;
    nvs_set_str(h, "thing", argv[1]);
    nvs_commit(h);
    nvs_close(h);
    strlcpy(g_zc_aws_thing, argv[1], sizeof(g_zc_aws_thing));
    printf("thing name saved: %s\n", g_zc_aws_thing);
    return 0;
}

static int aws_status_cmd(int argc, char **argv)
{
    (void)argc; (void)argv;
    printf("endpoint : %s:%d\n", g_zc_aws_endpoint[0] ? g_zc_aws_endpoint : "(unset)",
           (int)g_zc_aws_port);
    printf("thing    : %s\n", g_zc_aws_thing);
    printf("cert     : %s\n", g_zc_aws_cert ? "present" : "(unset)");
    printf("key      : %s\n", g_zc_aws_key ? "present" : "(unset)");
    printf("root CA  : %s\n", g_zc_aws_rootca ? "override in NVS" : "Amazon Root CA 1 (built in)");
    printf("wifi     : %s\n", g_zc_aws_wifi_owned ? "owned by aws_iot" : "owned elsewhere");
    printf("network  : %s\n", g_zc_aws_net_up ? "up" : "down");
    printf("session  : %s\n", g_zc_aws_connected ? "connected" : "disconnected");
    printf("subs     : %u\n", (unsigned)zc_aws_subs_count());
    for (size_t i = 0; ; i++) {
        const char *f = NULL;
        uint16_t len = 0;
        uint8_t qos = 0;
        if (!zc_aws_subs_get(i, &f, &len, &qos)) break;
        printf("  qos%u  %.*s\n", (unsigned)qos, (int)len, f);
    }
    return 0;
}

static int aws_creds_clear_cmd(int argc, char **argv)
{
    (void)argc; (void)argv;
    nvs_handle_t h;
    if (nvs_open("zc_aws", NVS_READWRITE, &h) != ESP_OK) return 1;
    nvs_erase_key(h, "cert");
    nvs_erase_key(h, "key");
    nvs_erase_key(h, "rootca");
    nvs_commit(h);
    nvs_close(h);
    free(g_zc_aws_cert);   g_zc_aws_cert = NULL;   g_zc_aws_cert_len = 0;
    free(g_zc_aws_key);    g_zc_aws_key = NULL;    g_zc_aws_key_len = 0;
    free(g_zc_aws_rootca); g_zc_aws_rootca = NULL; g_zc_aws_rootca_len = 0;
    printf("credentials cleared\n");
    return 0;
}

static int aws_wifi_cmd(int argc, char **argv)
{
    if (argc < 3) {
        printf("usage: aws-wifi <ssid> <password>\n");
        return 1;
    }
    nvs_handle_t h;
    if (nvs_open("zc_aws", NVS_READWRITE, &h) != ESP_OK) return 1;
    nvs_set_str(h, "ssid", argv[1]);
    nvs_set_str(h, "pass", argv[2]);
    nvs_commit(h);
    nvs_close(h);
    strlcpy(g_zc_aws_ssid, argv[1], sizeof(g_zc_aws_ssid));
    strlcpy(g_zc_aws_pass, argv[2], sizeof(g_zc_aws_pass));
    if (g_zc_aws_wifi_owned) {
        printf("wifi config saved — connecting\n");
        zc_aws_wifi_reconnect();
    } else {
        printf("wifi config saved, but another framework owns the radio here —"
               " configure Wi-Fi through it instead\n");
    }
    return 0;
}

void zc_aws_console_register(void)
{
    /* Filled field-by-field from a plain table, the same way app_console fills
     * its built-ins: newer IDF adds members to esp_console_cmd_t and the build
     * runs with -Werror=missing-field-initializers. */
    const struct { const char *command; const char *help; esp_console_cmd_func_t func; } cmds[] = {
        { "aws-wifi",
          "Set Wi-Fi credentials: aws-wifi <ssid> <password>",              &aws_wifi_cmd        },
        { "aws-endpoint",
          "Set the AWS IoT data endpoint: aws-endpoint <host> [8883|443]",  &aws_endpoint_cmd    },
        { "aws-thing",
          "Set the AWS IoT thing name: aws-thing <name>",                   &aws_thing_cmd       },
        { "aws-cert",
          "Paste the client certificate PEM (run it, then paste)", &aws_cred_cmd  },
        { "aws-key",
          "Paste the private key PEM (run it, then paste)",  &aws_cred_cmd        },
        { "aws-rootca",
          "Paste a root CA override PEM (run it, then paste)", &aws_cred_cmd      },
        { "aws-status",
          "Show AWS IoT config, network and session state, and subscriptions", &aws_status_cmd   },
        { "aws-creds-clear",
          "Erase the stored certificate, private key and root CA override", &aws_creds_clear_cmd },
    };

    for (const auto &c : cmds) {
        esp_console_cmd_t cmd = {};
        cmd.command = c.command;
        cmd.help = c.help;
        cmd.func = c.func;
        app_console_register_cmd(&cmd);
    }
}
