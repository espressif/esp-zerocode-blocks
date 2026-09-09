/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include <esp_err.h>
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/** One note of a pattern: freq_hz (0 = rest), duration ms. */
typedef struct { uint16_t freq_hz; uint16_t ms; } zc_tone_t;
/** Play a tone pattern (copied; async; latest wins — a new pattern replaces
 *  the current one, which is how alerts should behave). Silently a no-op when
 *  the product has no speaker instance. */
esp_err_t app_sound_play(const zc_tone_t *pattern, size_t count);
/** Bring up the sequencer task. Never blocks boot. */
esp_err_t app_sound_init(void);
#ifdef __cplusplus
}
#endif
