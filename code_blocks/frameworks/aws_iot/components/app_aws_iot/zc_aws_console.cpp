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
 *
 * Every command here is a thin wrapper: the storage is the public setters in
 * zc_aws_config.cpp, and reading a line or a paste is app_console's. aws-setup
 * is the same setters asked for one at a time — so "the device asks for its
 * credentials" costs a product nothing, and a product that wants a different
 * flow writes one console command over the same calls.
 */

#include "zc_aws_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_console.h>
#include <esp_err.h>
#include <esp_log.h>
#include <mbedtls/platform_util.h>

#include "app_console.h"

/* PASTE MODE: a PEM cannot be a command argument — the REPL ends a command at
 * the newline, so a 20-line certificate would arrive as 20 commands. Reading
 * the console from inside the handler works because the REPL is blocked here
 * until we return; app_console_read_text() is that reader, shared by any
 * command that takes multi-line or long input — here told where a PEM begins
 * and ends.
 *
 * It is the better path in every measurable way, which is why it is the only
 * one. A PEM's own 64-character line wrapping sits far below the ~246
 * characters at which USB-Serial-JTAG silently truncates a line, so the
 * truncation that corrupts chunked base64 cannot arise; and base64-ing a PEM
 * encodes an already-base64 body a second time, costing a third more bytes. */
static int cred_paste(const char *tag)
{
    char prompt[128];
    snprintf(prompt, sizeof(prompt),
             "%s: paste the PEM now, including the BEGIN/END lines.\n"
             "     (ends by itself at the -----END line; 60 s to start)\n", tag);
    app_console_text_opts_t pem = {};
    pem.begin = "-----BEGIN";
    pem.end = "-----END";
    char *text = NULL;
    size_t len = 0;
    if (app_console_read_text(prompt, &pem, &text, &len) != ESP_OK) {
        return 1;   /* read_text printed why */
    }
    const char *pem_text = text;
    esp_err_t err;
    if (strcmp(tag, "cert") == 0)      err = app_aws_iot_set_cert(pem_text);
    else if (strcmp(tag, "key") == 0)  err = app_aws_iot_set_key(pem_text);
    else                               err = app_aws_iot_set_root_ca(pem_text);
    if (err == ESP_ERR_INVALID_ARG) {
        printf("%s: %u bytes pasted but they are not a valid %s\n",
               tag, (unsigned)(len - 1),
               (strcmp(tag, "key") == 0) ? "private key" : "certificate");
    } else if (err != ESP_OK) {
        printf("%s: NVS write failed (%s)\n", tag, esp_err_to_name(err));
    } else {
        printf("%s saved (%u bytes, pasted)\n", tag, (unsigned)(len - 1));
    }
    /* A private key must not outlive the call in freed heap. */
    mbedtls_platform_zeroize(text, len);
    free(text);
    return err == ESP_OK ? 0 : 1;
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
    long port = (argc > 2) ? strtol(argv[2], NULL, 10) : 8883;
    if (port != 8883 && port != 443) {
        printf("port must be 8883 (MQTT/TLS) or 443 (ALPN)\n");
        return 1;
    }
    esp_err_t err = app_aws_iot_set_endpoint(argv[1], (uint16_t)port);
    if (err != ESP_OK) {
        printf("endpoint not saved: %s\n", esp_err_to_name(err));
        return 1;
    }
    printf("endpoint saved: %s:%d\n", g_zc_aws_endpoint, (int)g_zc_aws_port);
    return 0;
}

static int aws_thing_cmd(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage: aws-thing <name>\n");
        return 1;
    }
    esp_err_t err = app_aws_iot_set_thing(argv[1]);
    if (err != ESP_OK) {
        printf("thing name not saved: %s\n", esp_err_to_name(err));
        return 1;
    }
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
    if (app_aws_iot_clear_credentials() != ESP_OK) return 1;
    printf("credentials cleared\n");
    return 0;
}

static void wifi_saved_note(void)
{
    if (g_zc_aws_wifi_owned) {
        printf("wifi config saved — connecting\n");
    } else {
        printf("wifi config saved, but another framework owns the radio here —"
               " configure Wi-Fi through it instead\n");
    }
}

static int aws_wifi_cmd(int argc, char **argv)
{
    if (argc < 3) {
        printf("usage: aws-wifi <ssid> <password>\n");
        return 1;
    }
    esp_err_t err = app_aws_iot_set_wifi(argv[1], argv[2]);
    if (err != ESP_OK) {
        printf("wifi config not saved: %s\n", esp_err_to_name(err));
        return 1;
    }
    wifi_saved_note();
    return 0;
}

/* ── aws-setup: the guided version of everything above ──────────────── */

#define SETUP_ANSWER_MS  120000   /* per question: a person may go and find a value */

/* Ask one question. Returns false when the person walked away (timeout) or the
 * console is gone; prints nothing more in that case — the caller stops. An
 * over-long answer is re-asked rather than stored truncated. */
static bool setup_ask(const char *prompt, char *buf, size_t len, bool secret)
{
    for (;;) {
        esp_err_t err = app_console_read_line(prompt, buf, len, secret, SETUP_ANSWER_MS);
        if (err == ESP_OK) return true;
        if (err == ESP_ERR_INVALID_SIZE) {
            printf("  too long — at most %u characters\n", (unsigned)(len - 1));
            continue;
        }
        printf("aws-setup: no answer — stopped. Values already saved are kept; run aws-setup again to finish.\n");
        return false;
    }
}

/* A yes/no question: 1 yes, 0 no, -1 stopped (no answer). */
static int setup_yes(const char *prompt, bool dflt)
{
    char a[8];
    if (!setup_ask(prompt, a, sizeof(a), false)) return -1;
    if (a[0] == '\0') return dflt ? 1 : 0;
    return (a[0] == 'y' || a[0] == 'Y') ? 1 : 0;
}

/* Paste a PEM and store it through its setter, re-offering on a bad paste.
 * False only when the person stopped answering. */
static bool setup_pem(const char *tag, const char *label, bool present)
{
    char prompt[96];
    snprintf(prompt, sizeof(prompt), present ? "%s is stored — replace it? [y/N]: "
                                             : "%s — paste it now? [Y/n]: ", label);
    int yes = setup_yes(prompt, !present);
    if (yes <= 0) return yes == 0;                     /* kept / skipped, or stopped */
    for (int attempt = 0; attempt < 3; attempt++) {
        if (cred_paste(tag) == 0) return true;
        yes = setup_yes("  try the paste again? [Y/n]: ", true);
        if (yes <= 0) return yes == 0;
    }
    return true;
}

static int aws_setup_cmd(int argc, char **argv)
{
    (void)argc; (void)argv;
    char a[130], b[66], prompt[200];

    printf("aws-setup — answer each question; Enter keeps the value in [brackets].\n");

    /* Wi-Fi first, and only when this block owns the radio. The decision is made
     * on the session task ~2 s after boot; wait for it rather than guess. */
    for (int i = 0; i < 30 && !g_zc_aws_wifi_decided; i++) vTaskDelay(pdMS_TO_TICKS(100));
    if (!g_zc_aws_wifi_decided || g_zc_aws_wifi_owned) {
        snprintf(prompt, sizeof(prompt), "Wi-Fi network (SSID) [%s]: ", g_zc_aws_ssid);
        if (!setup_ask(prompt, a, 33, false)) return 1;
        bool keep_ssid = a[0] == '\0';
        if (keep_ssid && g_zc_aws_ssid[0] == '\0') {
            printf("  skipped — no network set yet\n");
        } else {
            if (!setup_ask(keep_ssid ? "Wi-Fi password [keep]: " : "Wi-Fi password: ", b, 65, true)) return 1;
            if (!(keep_ssid && b[0] == '\0')) {          /* both kept: nothing to write */
                /* The setter copies into the live config, so never hand it the
                 * live config itself as the source. */
                if (keep_ssid) strlcpy(a, g_zc_aws_ssid, sizeof(a));
                esp_err_t err = app_aws_iot_set_wifi(a, b);
                if (err != ESP_OK) printf("  not saved: %s\n", esp_err_to_name(err));
                else wifi_saved_note();
            }
        }
    } else {
        printf("Wi-Fi: owned by another framework here — onboard it through that one.\n");
    }

    for (;;) {
        snprintf(prompt, sizeof(prompt), "AWS IoT endpoint, host[:port] [%s:%d]: ",
                 g_zc_aws_endpoint[0] ? g_zc_aws_endpoint : "", (int)g_zc_aws_port);
        if (!setup_ask(prompt, a, sizeof(a), false)) return 1;
        if (a[0] == '\0') break;
        long port = 8883;
        char *colon = strrchr(a, ':');
        if (colon != NULL) { *colon = '\0'; port = strtol(colon + 1, NULL, 10); }
        if (port != 8883 && port != 443) { printf("  port must be 8883 or 443\n"); continue; }
        esp_err_t err = app_aws_iot_set_endpoint(a, (uint16_t)port);
        if (err == ESP_OK) { printf("  endpoint saved: %s:%d\n", g_zc_aws_endpoint, (int)g_zc_aws_port); break; }
        printf("  not saved: %s\n", esp_err_to_name(err));
    }

    snprintf(prompt, sizeof(prompt), "Thing name [%s]: ", g_zc_aws_thing);
    if (!setup_ask(prompt, a, 65, false)) return 1;
    if (a[0] != '\0') {
        esp_err_t err = app_aws_iot_set_thing(a);
        if (err == ESP_OK) printf("  thing name saved: %s\n", g_zc_aws_thing);
        else printf("  not saved: %s\n", esp_err_to_name(err));
    }

    if (!setup_pem("cert", "Device certificate", g_zc_aws_cert != NULL)) return 1;
    if (!setup_pem("key", "Private key", g_zc_aws_key != NULL)) return 1;

    if (app_aws_iot_configured()) {
        printf("aws-setup: done — connecting to %s:%d as \"%s\". aws-status shows the session.\n",
               g_zc_aws_endpoint, (int)g_zc_aws_port, g_zc_aws_thing);
    } else {
        printf("aws-setup: saved, but still missing:%s%s%s — run aws-setup again to finish.\n",
               g_zc_aws_endpoint[0] ? "" : " endpoint",
               g_zc_aws_cert ? "" : " certificate",
               g_zc_aws_key ? "" : " key");
    }
    return 0;
}

void zc_aws_console_register(void)
{
    /* Filled field-by-field from a plain table, the same way app_console fills
     * its built-ins: newer IDF adds members to esp_console_cmd_t and the build
     * runs with -Werror=missing-field-initializers. */
    const struct { const char *command; const char *help; esp_console_cmd_func_t func; } cmds[] = {
        { "aws-setup",
          "Guided setup: asks for Wi-Fi, endpoint, thing name, then the certificate and key", &aws_setup_cmd },
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
