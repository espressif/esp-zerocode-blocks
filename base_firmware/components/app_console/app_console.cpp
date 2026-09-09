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
