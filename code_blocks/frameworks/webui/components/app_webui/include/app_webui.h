/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * ESP ZeroCode AI - Web UI Framework
 *
 * BASE: No-op stub. The generator replaces app_webui.cpp with the real
 * implementation when the product has device-type bindings.
 */

#pragma once

#include <esp_err.h>
#include <esp_http_server.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t app_webui_init(void);

/* The page's server on port 80 (NULL before app_webui_init or when it failed). */
httpd_handle_t app_webui_server(void);

/* PRODUCT HOOK: register the product's own pages or endpoints on the page's
 * server — httpd_register_uri_handler(server, …) for each, under a path of the
 * product's own (/api/state, /api/set and / are the page's). Defined in a file
 * under components/app_logic; runs once, right after the page's own handlers.
 * There is one server on port 80: a second httpd_start cannot bind. A setting
 * or reading the page should simply show is a behaviors/webui_row instead. */
void app_webui_register_handlers(httpd_handle_t server);

#ifdef __cplusplus
}
#endif
