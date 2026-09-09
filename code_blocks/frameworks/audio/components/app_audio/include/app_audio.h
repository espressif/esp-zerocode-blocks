/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * ESP ZeroCode AI - Voice (esp-sr) Framework
 *
 * BASE: No-op stub. The generator replaces app_audio.cpp with the real
 * implementation when the product has device-type bindings.
 */

#pragma once

#include <esp_err.h>
#include <stddef.h>
#include <stdint.h>

esp_err_t app_audio_init(void);

/**
 * Raw PCM source for the AFE feed task, provided by drivers/voice_mic_i2s.
 *
 * Declared here rather than in the mic block so app_audio.cpp has a prototype
 * regardless of instance prefix — a product carries at most one voice mic, so
 * the name is fixed by contract. Blocks that provide it define it in their
 * audio_statics slot.
 *
 * Must block until `samples` int16 samples have been written, and return the
 * count actually produced (< samples signals a read error).
 */
size_t zc_audio_mic_read(int16_t *dest, size_t samples);
