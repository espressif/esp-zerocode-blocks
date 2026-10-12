/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * Wi-Fi onboarding over the serial console, and the session's status.
 *
 * The AWS identity (endpoint, thing name, certificate, key, root CA) is device
 * data written at flash time, so there is no command for it. The commands here
 * are thin wrappers: the storage is app_aws_iot_set_wifi in zc_aws_config.cpp,
 * and reading a line is app_console's.
 */

#include "zc_aws_internal.h"

#include <stdio.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_console.h>
#include <esp_err.h>

#include "app_console.h"

static int aws_status_cmd(int argc, char **argv)
{
    (void)argc; (void)argv;
    printf("endpoint : %s:%d\n", g_zc_aws_endpoint[0] ? g_zc_aws_endpoint : "(not flashed)",
           (int)g_zc_aws_port);
    printf("thing    : %s\n", g_zc_aws_thing);
    printf("cert     : %s\n", g_zc_aws_cert ? "present" : "(not flashed)");
    printf("key      : %s\n", g_zc_aws_key ? "present" : "(not flashed)");
    printf("root CA  : %s\n", g_zc_aws_rootca ? "flashed override" : "Amazon Root CA 1 (built in)");
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

/* ── aws-setup: the guided version of aws-wifi ───────────────────────── */

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
        printf("aws-setup: no answer — stopped. Run aws-setup again to finish.\n");
        return false;
    }
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

    if (app_aws_iot_configured()) {
        printf("aws-setup: done — connecting to %s:%d as \"%s\". aws-status shows the session.\n",
               g_zc_aws_endpoint, (int)g_zc_aws_port, g_zc_aws_thing);
    } else {
        printf("aws-setup: the device was flashed without%s%s%s — reflash it with its device data.\n",
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
          "Guided setup: asks for the Wi-Fi network and password",         &aws_setup_cmd       },
        { "aws-wifi",
          "Set Wi-Fi credentials: aws-wifi <ssid> <password>",              &aws_wifi_cmd        },
        { "aws-status",
          "Show AWS IoT config, network and session state, and subscriptions", &aws_status_cmd   },
    };

    for (const auto &c : cmds) {
        esp_console_cmd_t cmd = {};
        cmd.command = c.command;
        cmd.help = c.help;
        cmd.func = c.func;
        app_console_register_cmd(&cmd);
    }
}
