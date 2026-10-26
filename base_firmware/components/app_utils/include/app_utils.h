/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * ESP ZeroCode AI - Utility Functions
 *
 * Lightweight utilities with no app-level dependencies.
 * Safe to include from ANY component without circular dependency risk.
 */

#pragma once

#include <stdint.h>
#include <esp_err.h>
#include <driver/ledc.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Print current heap memory stats with a label.
 * Call at strategic points for memory analysis during testing.
 *
 * Output format: MEM [label]: free=XXXXX min=XXXXX largest=XXXXX
 */
void app_utils_print_mem(const char *label);

/**
 * Configure an LEDC timer at the HIGHEST duty resolution its clock reaches for
 * cfg->freq_hz, probing from the chip's widest (SOC_LEDC_TIMER_BIT_WIDTH) down.
 * A fixed resolution is not portable: 13-bit at 20 kHz needs a ~164 MHz clock,
 * which an ESP32-H2 does not have.
 *
 * cfg->duty_resolution is ignored on input and holds the chosen resolution on
 * return. Logs the choice. When no resolution works for the frequency it logs
 * that and returns ESP_ERR_NOT_SUPPORTED — it never aborts.
 *
 * @param cfg       timer config (speed_mode, timer_num, freq_hz, clk_cfg set)
 * @param max_duty  out: full-scale duty at the chosen resolution, 2^bits - 1
 *                  (0 on failure)
 */
esp_err_t app_utils_ledc_timer_config(ledc_timer_config_t *cfg, uint32_t *max_duty);

/**
 * PRODUCT HOOK: runs at the start of EVERY factory reset — the console's
 * `factory_reset` command and every factory-reset button, combo or power-cycle
 * block — before the NVS partition is erased and the device reboots.
 *
 * The default does nothing. To clear more on a factory reset (an NVS partition
 * other than the default one, such as the fctry partition holding device data,
 * or anything a product keeps outside NVS), define it in product code:
 *
 *     void app_factory_reset_hook(void) { nvs_flash_erase_partition("fctry"); }
 *
 * in a file under components/app_logic/ (linked whole, so a file holding only
 * this definition is still linked), with C linkage: a .cpp file must include
 * this header (or declare it extern "C"), or the definition is a different,
 * mangled symbol and the hook silently never runs. Never register a second
 * `factory_reset` console command: the console keeps only one of two commands
 * with the same name.
 *
 * It may run on any task (a button's timer task), at boot (a power-cycle
 * reset, before the product's own init) and on the Matter thread (removal
 * from the last fabric, just before esp_matter::factory_reset()), so it must
 * not block for long, wait on Matter work, or rely on the product being
 * initialized. Nothing it does can stop the reset.
 */
void app_factory_reset_hook(void);

/**
 * PRODUCT HOOKS: feedback while a factory-reset button is HELD — every
 * hold-to-reset path calls them: board_button's factory_reset_ms,
 * factory_reset_long_press and factory_reset_button_combo.
 *
 *   app_factory_reset_hold_started()   the press (both buttons, for a combo)
 *       has lasted the block's cue time (its `reset_cue_ms`; see the block),
 *       so it is no longer a click or an ordinary long press: the reset is
 *       now being counted down. Show it (an LED full blue, a beep).
 *   app_factory_reset_hold_cancelled() the button was released before the
 *       reset fired (or the reset's NVS erase failed and the device carries
 *       on). Go back to normal.
 *
 * When the hold completes, app_factory_reset_hook() runs as for every reset
 * and the device reboots; cancelled is not called. A press released before
 * the cue calls neither. Each hold calls started at most once, and cancelled
 * only after started.
 *
 * The defaults do nothing. Define them like app_factory_reset_hook (a file
 * under components/app_logic/, C linkage — include this header in a .cpp).
 * Never unregister or re-register the button block's own callbacks to get
 * this: its click, long press and reset keep working only through them.
 *
 * They run on the esp_timer task (the button's timer task): keep them short
 * and never block — set a param, fire an event, notify a task.
 */
void app_factory_reset_hold_started(void);
void app_factory_reset_hold_cancelled(void);

/**
 * The factory reset every path shares: app_factory_reset_hook(), erase the
 * default NVS partition (Wi-Fi, pairing, every saved setting), reboot.
 * Returns only if the erase fails, with its error; the device is not rebooted.
 */
esp_err_t app_utils_factory_reset(void);

#ifdef __cplusplus
}
#endif
