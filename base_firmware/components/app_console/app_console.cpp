/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * ESP ZeroCode AI - Device Console
 *
 * Always-on built-in commands (intentionally minimal — every product gets
 * these and nothing else by default):
 *   reboot          — software restart
 *   factory_reset   — wipe NVS + reboot (drops Matter commissioning)
 *   mem             — quick heap free / largest block
 *
 * Richer commands (uptime, chip info, version, log-level, detailed heap,
 * param get/set, matter diag, etc.) are opt-in blocks under
 * templates/blocks/behaviors/console_*. Products select what they need.
 */

#include "app_console.h"
#include "app_config.h"
#include "app_utils.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>

static const char *TAG = "app_console";

/* ── Built-in command handlers ───────────────────────────────────────── */

static int cmd_reboot(int argc, char **argv)
{
    ESP_LOGI(TAG, "Rebooting...");
    esp_restart();
    return 0; /* unreachable */
}

static int cmd_factory_reset(int argc, char **argv)
{
    ESP_LOGW(TAG, "Factory reset: erasing NVS...");
    esp_err_t err = nvs_flash_erase();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS erase failed: %s", esp_err_to_name(err));
        return 1;
    }
    ESP_LOGI(TAG, "NVS erased. Rebooting...");
    esp_restart();
    return 0; /* unreachable */
}

static int cmd_mem(int argc, char **argv)
{
    app_utils_print_mem("console");
    return 0;
}

/* ── Public API ──────────────────────────────────────────────────────── */

esp_err_t app_console_register_cmd(const esp_console_cmd_t *cmd)
{
    return esp_console_cmd_register(cmd);
}

static esp_err_t register_builtin_cmds(void)
{
    /* Filled field-by-field from a plain table: newer IDF adds members to
     * esp_console_cmd_t and the build runs with
     * -Werror=missing-field-initializers. */
    const struct { const char *command; const char *help; esp_console_cmd_func_t func; } builtins[] = {
        { "reboot",        "Software reboot",                 &cmd_reboot        },
        { "factory_reset", "Erase NVS and reboot",            &cmd_factory_reset },
        { "mem",           "Print heap free / largest block", &cmd_mem           },
    };

    for (const auto &b : builtins) {
        esp_console_cmd_t cmd = {};
        cmd.command = b.command;
        cmd.help = b.help;
        cmd.func = b.func;
        esp_err_t err = esp_console_cmd_register(&cmd);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register '%s': %s", b.command, esp_err_to_name(err));
            return err;
        }
    }
    return ESP_OK;
}

esp_err_t app_console_init(void)
{
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = APP_PRODUCT_NAME ">";

    /* Register commands into the global registry before the transport starts. */
    esp_err_t err = register_builtin_cmds();
    if (err != ESP_OK) {
        return err;
    }

    /* Bring up the REPL on whichever transport menuconfig selected as the
     * primary console. Exactly one is active, so only that one is registered. */
    esp_console_repl_t *repl = NULL;
#if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    err = esp_console_new_repl_uart(&hw_config, &repl_config, &repl);
#elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
    esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    err = esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl);
#elif defined(CONFIG_ESP_CONSOLE_NONE)
    /* Console deliberately OFF (a product may free the UART pins for its own
     * hardware). No REPL to start; the command registry stays populated so a
     * later transport could still use it. Not an error — this component is in
     * every product, so an #error here would make NONE unbuildable fleet-wide. */
    ESP_LOGI(TAG, "primary console is NONE — REPL not started");
    return ESP_OK;
#else
    /* USB_CDC (S2-era USB-OTG console) and anything newer: fail the COMPILE
     * loudly rather than boot with a silently dead console. Add the transport
     * here when a product actually needs it. */
#error "Unsupported primary console transport"
#endif
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create console REPL: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_console_start_repl(repl);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start REPL: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Console ready. Type 'help' for commands.");
    return ESP_OK;
}

/* ── Reading input from inside a command ─────────────────────────────── */

#define CONSOLE_TEXT_MAX        8192    /* an RSA-4096 key PEM is ~3.2 KB */
#define CONSOLE_TEXT_TOTAL_MS   60000
#define CONSOLE_TEXT_IDLE_MS    8000
#define CONSOLE_CARRY_MAX       256
#define CONSOLE_CARRY_STALE_MS  2000

/* Bytes read past the end of one answer, kept for the next read in the SAME
 * command — a paced sender may put two answers in one burst, and read() has no
 * way to give bytes back. linenoise never sees this buffer, so whatever is left
 * when the command returns is dropped once it goes stale rather than answering
 * a prompt of some later command. */
static char s_carry[CONSOLE_CARRY_MAX];
static size_t s_carry_len = 0;
static TickType_t s_carry_at = 0;
/* The last line ended on CR: a LF straight after it is the same Enter. */
static bool s_skip_lf = false;

static bool console_present(void)
{
#if defined(CONFIG_ESP_CONSOLE_NONE)
    return false;
#else
    return true;
#endif
}

/* At least one tick, always: pdMS_TO_TICKS() rounds DOWN, so anything under one
 * tick is vTaskDelay(0), which never blocks — the loop then spins, IDLE never
 * runs, and the task watchdog aborts the board mid-input. */
static void console_poll_delay(void)
{
    TickType_t t = pdMS_TO_TICKS(10);
    vTaskDelay(t > 0 ? t : 1);
}

static bool elapsed(TickType_t since, uint32_t ms)
{
    return (xTaskGetTickCount() - since) >= pdMS_TO_TICKS(ms);
}

/* Put bytes back IN FRONT of whatever is still carried. */
static void carry_unread(const char *data, size_t n)
{
    if (n == 0) return;
    if (n + s_carry_len > CONSOLE_CARRY_MAX) {
        n = CONSOLE_CARRY_MAX - s_carry_len;   /* a burst this large is not typing */
    }
    memmove(s_carry + n, s_carry, s_carry_len);
    memcpy(s_carry, data, n);
    s_carry_len += n;
    s_carry_at = xTaskGetTickCount();
}

/* Non-blocking read: the carry first, then the console. <= 0 means nothing yet. */
static int console_read(char *rx, size_t n)
{
    if (s_carry_len > 0) {
        size_t m = n < s_carry_len ? n : s_carry_len;
        memcpy(rx, s_carry, m);
        memmove(s_carry, s_carry + m, s_carry_len - m);
        s_carry_len -= m;
        return (int)m;
    }
    /* Bulk reads, not fgetc per character: the receive buffer is small and a
     * sender that outruns a per-character loop loses bytes silently. */
    return read(STDIN_FILENO, rx, n);
}

/* Wipe input that may be a secret (a pasted private key) before its buffer is
 * freed: volatile stores, so the compiler cannot drop them as dead. */
static void wipe(void *p, size_t n)
{
    volatile unsigned char *v = (volatile unsigned char *)p;
    while (n--) *v++ = 0;
}

/* O_NONBLOCK on stdin for the duration of one read, restored on every exit. */
struct console_nonblocking {
    int flags;
    console_nonblocking() : flags(fcntl(STDIN_FILENO, F_GETFL, 0)) { fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK); }
    ~console_nonblocking() { fcntl(STDIN_FILENO, F_SETFL, flags); }
};

/* Line terminators already waiting when a prompt is about to be printed were
 * sent BEFORE it, so they cannot be its answer: the usual one is the LF of the
 * CRLF that ended the command line itself, which would otherwise make the
 * first prompt of every command answer itself with an empty line. */
static void drop_stale_input(void)
{
    if (s_carry_len > 0 && elapsed(s_carry_at, CONSOLE_CARRY_STALE_MS)) {
        s_carry_len = 0;
    }
    char rx[64];
    for (;;) {
        int got = console_read(rx, sizeof(rx));
        if (got <= 0) return;
        int i = 0;
        while (i < got && (rx[i] == '\r' || rx[i] == '\n')) i++;
        if (i < got) {
            carry_unread(rx + i, (size_t)(got - i));
            s_skip_lf = false;
            return;
        }
    }
}

esp_err_t app_console_read_line(const char *prompt, char *buf, size_t len,
                                bool secret, uint32_t timeout_ms)
{
    if (buf == NULL || len < 2) return ESP_ERR_INVALID_ARG;
    buf[0] = '\0';
    if (!console_present()) return ESP_ERR_NOT_SUPPORTED;

    console_nonblocking nb;
    drop_stale_input();
    if (prompt != NULL) fputs(prompt, stdout);
    fflush(stdout);

    size_t n = 0;
    bool overflow = false;
    int esc = 0;   /* 1 = after ESC, 2 = inside an ESC [ … sequence (arrow keys) */
    TickType_t start = xTaskGetTickCount();
    char rx[64];

    for (;;) {
        int got = console_read(rx, sizeof(rx));
        if (got <= 0) {
            if (timeout_ms != 0 && elapsed(start, timeout_ms)) {
                buf[n] = '\0';
                fputs("\n", stdout);
                fflush(stdout);
                return ESP_ERR_TIMEOUT;
            }
            console_poll_delay();
            continue;
        }
        for (int i = 0; i < got; i++) {
            char c = rx[i];
            if (s_skip_lf) {
                s_skip_lf = false;
                if (c == '\n') continue;
            }
            if (esc == 1) { esc = (c == '[' || c == 'O') ? 2 : 0; continue; }
            if (esc == 2) { if (c >= 0x40 && c <= 0x7e) esc = 0; continue; }
            if (c == '\r' || c == '\n') {
                s_skip_lf = (c == '\r');
                carry_unread(rx + i + 1, (size_t)(got - i - 1));
                buf[n] = '\0';
                fputs("\n", stdout);
                fflush(stdout);
                return overflow ? ESP_ERR_INVALID_SIZE : ESP_OK;
            }
            if (c == 0x08 || c == 0x7f) {           /* backspace / DEL */
                if (n > 0) { n--; fputs("\b \b", stdout); }
                continue;
            }
            if (c == 0x1b) { esc = 1; continue; }
            if ((unsigned char)c < 0x20) continue;  /* other control characters */
            if (n + 1 >= len) {                     /* full: refuse, audibly */
                overflow = true;
                fputc('\a', stdout);
                continue;
            }
            buf[n++] = c;
            fputc(secret ? '*' : c, stdout);
        }
        fflush(stdout);
    }
}

/* Does s[0..n) start with prefix? */
static bool starts_with(const char *s, size_t n, const char *prefix)
{
    size_t p = strlen(prefix);
    return n >= p && memcmp(s, prefix, p) == 0;
}

esp_err_t app_console_read_text(const char *prompt, const app_console_text_opts_t *opts,
                                char **out, size_t *out_len)
{
    if (out == NULL || out_len == NULL) return ESP_ERR_INVALID_ARG;
    *out = NULL;
    *out_len = 0;
    if (!console_present()) return ESP_ERR_NOT_SUPPORTED;

    const app_console_text_opts_t dflt = {};
    const app_console_text_opts_t *o = opts ? opts : &dflt;
    const size_t max = o->max_bytes ? o->max_bytes : CONSOLE_TEXT_MAX;
    const uint32_t timeout_ms = o->timeout_ms ? o->timeout_ms : CONSOLE_TEXT_TOTAL_MS;
    const uint32_t idle_ms = o->idle_ms ? o->idle_ms : CONSOLE_TEXT_IDLE_MS;

    char *buf = (char *)malloc(max + 1);
    if (buf == NULL) return ESP_ERR_NO_MEM;

    console_nonblocking nb;
    drop_stale_input();
    if (prompt != NULL) fputs(prompt, stdout);
    fflush(stdout);

    /* Everything goes into buf line by line; a line is judged when its newline
     * arrives. Before `begin`, a judged line that does not match is discarded
     * (len rewinds to its start), which is how anything ahead of the input —
     * a terminal's echo, the command line's own newline — falls away. */
    size_t len = 0, line_start = 0;
    bool started = false, done = false, overflow = false;
    /* A line ends at LF, at CRLF, or at a lone CR: several terminals (macOS
     * Terminal and iTerm under screen, among others) turn a pasted newline
     * into CR, and dropping every CR left such a paste waiting for a newline
     * that never comes. after_cr swallows the LF of a CRLF. */
    bool after_cr = false;
    TickType_t start = xTaskGetTickCount(), last = start;
    char rx[128];

    while (!done) {
        int n = console_read(rx, sizeof(rx));
        if (n <= 0) {
            if (len == 0 && !started && elapsed(start, timeout_ms)) break;
            if ((len > 0 || started) && elapsed(last, idle_ms)) break;
            console_poll_delay();
            continue;
        }
        last = xTaskGetTickCount();
        for (int i = 0; i < n && !done; i++) {
            char c = rx[i];
            if (after_cr) {
                after_cr = false;
                if (c == '\n') continue;                       /* the LF of a CRLF */
            }
            if (c == '\r') { after_cr = true; c = '\n'; }
            if (o->echo) fputc(c, stdout);
            if (len >= max) { overflow = true; done = true; break; }
            buf[len++] = c;
            if (c != '\n') continue;

            const char *line = buf + line_start;
            size_t line_len = len - line_start - 1;          /* without the '\n' */
            if (!started) {
                bool opens = o->begin ? starts_with(line, line_len, o->begin) : line_len > 0;
                if (!opens) { len = line_start; continue; }  /* not the input yet */
                started = true;
            }
            if (o->end != NULL ? starts_with(line, line_len, o->end) : line_len == 0) {
                if (o->end == NULL) len = line_start;          /* the empty line is not kept */
                done = true;
                carry_unread(rx + i + 1, (size_t)(n - i - 1));
            }
            line_start = len;
        }
        if (o->echo) fflush(stdout);
    }
    /* Ended on a CR: its LF, if one follows, belongs to this input, not to the
     * next read_line's answer. */
    if (done && after_cr) s_skip_lf = true;

    if (overflow) {
        printf("more than %u bytes — input refused\n", (unsigned)max);
        wipe(buf, len);
        free(buf);
        return ESP_ERR_INVALID_SIZE;
    }
    if (!done) {
        printf(started || len > 0 ? "input ended before its last line (%u bytes read) — nothing kept\n"
                                  : "no input arrived — nothing kept\n", (unsigned)len);
        wipe(buf, len);
        free(buf);
        return ESP_ERR_TIMEOUT;
    }
    if (o->join_lines) {
        size_t w = 0;
        for (size_t r = 0; r < len; r++) if (buf[r] != '\n') buf[w++] = buf[r];
        len = w;
    } else if (o->end == NULL && len > 0 && buf[len - 1] == '\n') {
        len--;                                               /* the last line's newline */
    }
    buf[len] = '\0';
    *out = buf;
    *out_len = len + 1;
    return ESP_OK;
}
