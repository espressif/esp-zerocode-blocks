/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * Host test: app_driver_run_serialized() runs product decisions one at a time
 * on one task, whichever tasks wrote the params that asked for them.
 *
 * Listeners run on the WRITER's task, so a thermostat decision (read target +
 * temperature, write the relay) called straight from a listener runs on two
 * tasks at once when two of them write, and the older decision can land last.
 * Here two threads write concurrently, the listener only asks for the
 * decision, and the decision must never overlap itself and must end on the
 * final inputs. Also: a decision that writes a param and asks for itself again
 * (re-entry from the serial task) neither deadlocks nor runs nested, repeated
 * requests while one is pending merge into one run, and a full request table
 * is refused rather than dropped silently.
 * Compiled natively with shims: std::thread tasks, real locks (see run.sh).
 */
#include "app_driver.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

esp_err_t app_driver_apply_param(app_driver_param_id_t, app_driver_param_val_t) { return ESP_OK; }

static int s_failures = 0;
#define CHECK(cond, msg) do { \
    if (cond) { printf("  ok   %s\n", msg); } \
    else { printf("  FAIL %s\n", msg); s_failures++; } \
} while (0)

static app_driver_param_val_t B(bool b) { app_driver_param_val_t v; memset(&v, 0, sizeof(v)); v.b = b; return v; }
static app_driver_param_val_t U8(uint8_t u) { app_driver_param_val_t v; memset(&v, 0, sizeof(v)); v.u8 = u; return v; }
static app_driver_param_val_t U16(uint16_t u) { app_driver_param_val_t v; memset(&v, 0, sizeof(v)); v.u16 = u; return v; }

/* LEVEL = temperature, LED_PATTERN = target, POWER = heater relay. */
static std::atomic<int> s_in_flight{0};
static std::atomic<int> s_max_in_flight{0};
static std::atomic<int> s_runs{0};

static void decide(void *) {
    int now = ++s_in_flight;
    int prev = s_max_in_flight.load();
    while (now > prev && !s_max_in_flight.compare_exchange_weak(prev, now)) {}
    s_runs++;
    app_driver_param_val_t temp, target;
    app_driver_get_param(APP_DRIVER_PARAM_LEVEL, &temp);
    app_driver_get_param(APP_DRIVER_PARAM_LED_PATTERN, &target);
    std::this_thread::sleep_for(std::chrono::microseconds(200)); /* widen the window */
    app_driver_set_param(APP_DRIVER_PARAM_POWER, B(temp.u16 < target.u8), APP_DRIVER_SOURCE_LOCAL);
    --s_in_flight;
}

static void product_cb(app_driver_param_id_t id, app_driver_param_val_t, app_driver_handle_t, void *) {
    if (id == APP_DRIVER_PARAM_LEVEL || id == APP_DRIVER_PARAM_LED_PATTERN) {
        app_driver_run_serialized(decide, NULL);
    }
}

/* Wait until every request posted so far has run: post a marker and wait. */
static std::atomic<bool> s_marker{false};
static void marker(void *) { s_marker = true; }
static bool drain(void) {
    s_marker = false;
    esp_err_t err = ESP_ERR_NO_MEM;
    for (int i = 0; i < 5000 && err == ESP_ERR_NO_MEM; i++) { /* a full table frees up as it runs */
        err = app_driver_run_serialized(marker, NULL);
        if (err == ESP_ERR_NO_MEM) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (err != ESP_OK) return false;
    for (int i = 0; i < 5000 && !s_marker; i++) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    return s_marker;
}

/* Re-entry: a decision on the serial task writes a param and asks for a second
 * decision; the second runs after the first returns, never inside it. */
static std::atomic<int> s_order{0};
static std::atomic<int> s_outer_done_at{0}, s_inner_ran_at{0};
static std::atomic<bool> s_inner_saw_outer_running{false};
static std::atomic<bool> s_outer_running{false};
static void inner(void *) {
    s_inner_saw_outer_running = s_outer_running.load();
    s_inner_ran_at = ++s_order;
}
static void outer(void *) {
    s_outer_running = true;
    app_driver_set_param(APP_DRIVER_PARAM_POWER, B(true), APP_DRIVER_SOURCE_LOCAL);
    esp_err_t err = app_driver_run_serialized(inner, NULL);
    (void)err;
    s_outer_running = false;
    s_outer_done_at = ++s_order;
}

/* Merge: while the serial task is held inside `gate`, the same request posted
 * many times runs once afterwards. */
static std::atomic<bool> s_gate_open{false}, s_gate_entered{false};
static std::atomic<int> s_counted{0};
static void gate(void *) { s_gate_entered = true; while (!s_gate_open) std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
static void counted(void *) { s_counted++; }

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0); /* a hang still shows how far it got */
    printf("app_driver run_serialized test\n");
    app_driver_register_solution("product", product_cb, NULL);

    /* Two writer tasks at once (a sensor poll and an MQTT/console target). */
    std::thread sensor([] {
        for (int i = 0; i < 300; i++) app_driver_set_param(APP_DRIVER_PARAM_LEVEL, U16(100 + (i % 50)), APP_DRIVER_SOURCE_LOCAL);
        app_driver_set_param(APP_DRIVER_PARAM_LEVEL, U16(120), APP_DRIVER_SOURCE_LOCAL);
    });
    std::thread remote([] {
        for (int i = 0; i < 300; i++) app_driver_set_param(APP_DRIVER_PARAM_LED_PATTERN, U8(100 + (i % 60)), 2);
        app_driver_set_param(APP_DRIVER_PARAM_LED_PATTERN, U8(110), 2);
    });
    sensor.join();
    remote.join();
    CHECK(drain(), "the serial task drains");
    app_driver_param_val_t relay;
    app_driver_get_param(APP_DRIVER_PARAM_POWER, &relay);
    CHECK(s_runs > 0, "decisions ran");
    CHECK(s_max_in_flight == 1, "a decision never overlaps another, whichever task wrote");
    CHECK(relay.b == false, "the relay ends on the final inputs (120 >= 110: off)");

    /* Re-entry from the serial task itself. */
    CHECK(app_driver_run_serialized(outer, NULL) == ESP_OK, "a request is accepted");
    CHECK(drain(), "a decision that writes and asks again does not deadlock");
    CHECK(s_inner_ran_at > 0 && s_outer_done_at > 0 && s_inner_ran_at > s_outer_done_at,
          "the request made inside a decision runs after it");
    CHECK(!s_inner_saw_outer_running, "...never nested inside it");

    /* Merge while pending. */
    app_driver_run_serialized(gate, NULL);
    while (!s_gate_entered) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    for (int i = 0; i < 50; i++) app_driver_run_serialized(counted, NULL);
    s_gate_open = true;
    CHECK(drain(), "drains after the gate");
    CHECK(s_counted == 1, "fifty requests of one pending decision run it once");

    /* A full table refuses instead of dropping silently. */
    static int args[64];
    s_gate_open = false; s_gate_entered = false;
    app_driver_run_serialized(gate, NULL);
    while (!s_gate_entered) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    bool refused = false;
    for (int i = 0; i < 64 && !refused; i++) refused = app_driver_run_serialized(counted, &args[i]) == ESP_ERR_NO_MEM;
    s_gate_open = true;
    CHECK(refused, "distinct pending requests past the table size are refused with ESP_ERR_NO_MEM");
    CHECK(drain(), "drains after the refusal");
    CHECK(app_driver_run_serialized(NULL, NULL) == ESP_ERR_INVALID_ARG, "a NULL function is refused");

    printf(s_failures ? "\nRESULT: FAIL (%d)\n" : "\nRESULT: PASS\n", s_failures);
    return s_failures ? 1 : 0;
}
