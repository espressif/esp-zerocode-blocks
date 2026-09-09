/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * ESP ZeroCode AI - Vision Framework
 *
 * BASE: No-op stub. The generator replaces app_vision.cpp with the real
 * implementation when the product has device-type bindings.
 */

#pragma once

#include <esp_err.h>

esp_err_t app_vision_init(void);

/**
 * Detection event hook — DETECTOR DRIVER BLOCKS call this from their
 * capture/detect tasks; the generated implementation dispatches every
 * vision_event_cases slot with `event_id` and `value` in scope.
 *
 * Event vocabulary: 0 = motion (value 0/1). New detector blocks extend it.
 * The stub is a no-op so a detector block links even in a product that
 * somehow carries no bindings.
 */
void zc_vision_emit(int event_id, int value);
