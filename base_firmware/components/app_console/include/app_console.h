/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * ESP ZeroCode AI - Device Console Interface
 *
 * Provides an interactive CLI over the serial port with built-in
 * diagnostic commands (reboot, factory_reset, mem) and a registration
 * API for product-specific commands added by the AI.
 *
 * Uses ESP-IDF's esp_console component (UART or USB-Serial-JTAG REPL,
 * whichever is the primary console). On a board whose usb_hs_console owns
 * stdio (CONFIG_ESP_CONSOLE_NONE + CONFIG_USB_HS_CONSOLE_USB_CDC_AUTO_INIT),
 * the REPL runs on that TinyUSB CDC, without line editing or history.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <esp_err.h>
#include <esp_console.h>

/**
 * Initialize the console REPL and register built-in commands.
 * Call this AFTER esp_matter::start() so Matter is fully initialized.
 */
esp_err_t app_console_init(void);

/**
 * Register a custom console command.
 * Product-specific commands are registered from app_driver or app_logic.
 */
esp_err_t app_console_register_cmd(const esp_console_cmd_t *cmd);

/* ── Reading input from inside a command ───────────────────────────────
 *
 * A command that needs more than its argv — a guided setup that asks one
 * question at a time, a certificate pasted after the command — reads the
 * console with these two calls. They exist so that no product re-learns how:
 * non-blocking VFS reads, echo (linenoise is not running inside a handler, so
 * nothing is echoed unless we do it), CR / LF / CRLF senders, and a delay of
 * at least one tick so the idle task runs and the task watchdog stays quiet.
 *
 * CALL THEM ONLY FROM A CONSOLE COMMAND HANDLER. The REPL task is blocked in
 * the handler until it returns, which makes the handler the one reader of
 * stdin. A second task reading stdin while the REPL runs races it for every
 * byte — there is no safe way to do that, and no need: register a command.
 *
 * Both work on a UART and a USB-Serial-JTAG primary console alike (the REPL
 * installs the VFS driver they read through), and on a usb_hs_console CDC.
 * With CONFIG_ESP_CONSOLE_NONE and no usb_hs_console there is no console and
 * both return ESP_ERR_NOT_SUPPORTED.
 */

/**
 * Print @p prompt and read one typed line into @p buf.
 *
 * Typed characters are echoed (as '*' when @p secret), backspace edits, and
 * the line ends at Enter (CR, LF or CRLF — a CRLF counts once). Line
 * terminators left over from the command line itself are skipped, so the
 * first prompt of a command never answers itself. An empty line (just Enter)
 * is a valid answer: buf[0] == '\0'.
 *
 * @param buf         Receives the line, NUL-terminated, without the newline.
 * @param len         Size of @p buf, including the NUL.
 * @param timeout_ms  Give up after this long with no complete line; 0 = wait
 *                    for ever.
 * @return ESP_OK; ESP_ERR_TIMEOUT; ESP_ERR_INVALID_SIZE when more than
 *         len - 1 characters were typed (the extra ones were refused with a
 *         bell, and @p buf holds what fit — the caller decides, it is never
 *         a silent truncation); ESP_ERR_INVALID_ARG; ESP_ERR_NOT_SUPPORTED.
 */
esp_err_t app_console_read_line(const char *prompt, char *buf, size_t len,
                                bool secret, uint32_t timeout_ms);

/** How app_console_read_text knows where the input starts and ends. Zero
 *  fields take the defaults, so `{}` reads plain text up to an empty line. */
typedef struct {
    /** Skip whole lines until one STARTS WITH this, and begin there (that line
     *  is kept). NULL: begin at the first non-empty line. */
    const char *begin;
    /** Finish after a line that STARTS WITH this (kept). NULL: finish at an
     *  empty line (not kept). */
    const char *end;
    /** Drop every newline from the result: one long value — a token, a key,
     *  a base64 blob — pasted wrapped over several lines, for transports or
     *  terminals that split or truncate long lines. */
    bool join_lines;
    /** Echo what arrives. Off suits a paste, on suits typing. */
    bool echo;
    /** Most bytes accepted; 0 = 8192. */
    size_t max_bytes;
    /** How long to wait for the input to START; 0 = 60 s. */
    uint32_t timeout_ms;
    /** Silence that ends a started input as INCOMPLETE; 0 = 8 s. */
    uint32_t idle_ms;
} app_console_text_opts_t;

/**
 * Print @p prompt, then read multi-line (or long) input verbatim — a
 * certificate, a JSON document, a long token — that a command line cannot
 * carry: the REPL ends a command at the newline and the console's transport
 * may cut a long line.
 *
 * Examples:
 *   PEM:   { .begin = "-----BEGIN", .end = "-----END" }
 *   JSON:  { .begin = "{", .end = "}" }  (a pasted, pretty-printed object)
 *   token: { .join_lines = true }    (wrapped over lines, ended by an empty one)
 *
 * The sender should pace itself (about 20 ms per line): the console's receive
 * buffer is small and not flow-controlled. A line ends at LF, CRLF or a lone
 * CR, so terminals that send any of the three are fine.
 *
 * @param opts     NULL = all defaults.
 * @param out      Receives a malloc'd, NUL-terminated copy; the caller frees it.
 *                 NULL on any error.
 * @param out_len  Receives the length INCLUDING the NUL — what mbedTLS's PEM
 *                 parsers expect (passing strlen() makes them fail).
 * @return ESP_OK; ESP_ERR_TIMEOUT (nothing arrived, or the end never came);
 *         ESP_ERR_INVALID_SIZE (over max_bytes); ESP_ERR_NO_MEM;
 *         ESP_ERR_INVALID_ARG; ESP_ERR_NOT_SUPPORTED.
 */
esp_err_t app_console_read_text(const char *prompt, const app_console_text_opts_t *opts,
                                char **out, size_t *out_len);
