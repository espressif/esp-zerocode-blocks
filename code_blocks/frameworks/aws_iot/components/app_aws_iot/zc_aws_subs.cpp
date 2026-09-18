/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * The subscription registry.
 *
 * Subscriptions here are DECLARATIVE: a caller registers a filter once and the
 * session replays every registration on each reconnect where the broker reports
 * no surviving session. An imperative subscribe — send SUBSCRIBE, forget —
 * works until the first network blip and then goes silently deaf, which is a
 * failure generated code cannot observe and will never debug.
 *
 * Filter strings are COPIED. coreMQTT-Agent's own reference implementation
 * documents that the caller must keep the filter in scope until unsubscribed;
 * that is a trap for a caller that built the topic on its stack.
 */

#include "zc_aws_internal.h"

#include <stdlib.h>
#include <string.h>

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

typedef struct {
    char           *filter;      /* owned */
    uint16_t        filter_len;
    uint8_t         qos;
    zc_aws_msg_cb_t cb;
    void           *ctx;
} zc_aws_sub_t;

static zc_aws_sub_t      s_subs[ZC_AWS_MAX_SUBS];
static size_t            s_count;
static SemaphoreHandle_t s_lock;

/* Created once from app_aws_iot_init(), before the session task exists.
 * Creating it lazily on first use would race: the session task calls
 * zc_aws_subs_count() while the main task is still running behaviour init and
 * calling zc_aws_subs_add(). */
void zc_aws_subs_init(void)
{
    if (s_lock == NULL) s_lock = xSemaphoreCreateMutex();
}

static bool subs_lock(void)
{
    if (s_lock == NULL) return false;
    return xSemaphoreTake(s_lock, portMAX_DELAY) == pdTRUE;
}

static void subs_unlock(void)
{
    xSemaphoreGive(s_lock);
}

esp_err_t zc_aws_subs_add(const char *filter, uint8_t qos,
                          zc_aws_msg_cb_t cb, void *ctx)
{
    if (filter == NULL || filter[0] == '\0' || cb == NULL) return ESP_ERR_INVALID_ARG;
    if (qos > 1) return ESP_ERR_INVALID_ARG;   /* AWS IoT Core has no QoS 2 */

    size_t len = strlen(filter);
    if (len > UINT16_MAX) return ESP_ERR_INVALID_ARG;

    if (!subs_lock()) return ESP_ERR_NO_MEM;

    /* Re-registering the same filter+callback replaces the entry rather than
     * adding a duplicate, so an init path that runs twice cannot exhaust the
     * registry. */
    size_t slot = s_count;
    for (size_t i = 0; i < s_count; i++) {
        if (s_subs[i].cb == cb && strcmp(s_subs[i].filter, filter) == 0) {
            slot = i;
            break;
        }
    }
    if (slot == s_count && s_count >= ZC_AWS_MAX_SUBS) {
        subs_unlock();
        ESP_LOGE(ZC_AWS_TAG, "subscription registry full (%d) — cannot add \"%s\"",
                 ZC_AWS_MAX_SUBS, filter);
        return ESP_ERR_NO_MEM;
    }

    if (slot == s_count) {
        char *copy = strdup(filter);
        if (copy == NULL) {
            subs_unlock();
            return ESP_ERR_NO_MEM;
        }
        s_subs[slot].filter = copy;
        s_subs[slot].filter_len = (uint16_t)len;
        s_count++;
    }
    /* An existing slot keeps its filter allocation: the only way to land on one
     * is an identical filter string, so there is nothing to rewrite — and never
     * freeing a live entry's filter is what lets zc_aws_subs_get() hand the
     * pointer out and have it stay valid. */
    s_subs[slot].qos = qos;
    s_subs[slot].cb = cb;
    s_subs[slot].ctx = ctx;

    subs_unlock();
    ESP_LOGI(ZC_AWS_TAG, "subscribed \"%s\" (qos %u)", filter, (unsigned)qos);
    return ESP_OK;
}

size_t zc_aws_subs_count(void)
{
    if (!subs_lock()) return 0;
    size_t n = s_count;
    subs_unlock();
    return n;
}

bool zc_aws_subs_get(size_t i, const char **filter, uint16_t *len, uint8_t *qos)
{
    if (!subs_lock()) return false;
    bool ok = (i < s_count);
    if (ok) {
        /* Handing the filter pointer out past the lock is safe because a live
         * entry's allocation is never freed or replaced — see zc_aws_subs_add. */
        if (filter) *filter = s_subs[i].filter;
        if (len)    *len    = s_subs[i].filter_len;
        if (qos)    *qos    = s_subs[i].qos;
    }
    subs_unlock();
    return ok;
}

void zc_aws_subs_dispatch(MQTTPublishInfo_t *pub)
{
    if (pub == NULL) return;

    /* Matching happens UNDER the lock, but callbacks run outside it — a
     * callback that publishes would otherwise re-enter this mutex from the
     * agent task. Only the callback and its context are copied out: copying the
     * filter POINTER and matching afterwards would be a use-after-free the one
     * time zc_aws_subs_add() replaced an entry in between. */
    struct { zc_aws_msg_cb_t cb; void *ctx; } hits[ZC_AWS_MAX_SUBS];
    size_t nhits = 0;

    if (!subs_lock()) return;
    for (size_t i = 0; i < s_count; i++) {
        bool match = false;
        if (MQTT_MatchTopic(pub->pTopicName, pub->topicNameLength,
                            s_subs[i].filter, s_subs[i].filter_len,
                            &match) != MQTTSuccess) {
            continue;
        }
        if (!match) continue;
        hits[nhits].cb = s_subs[i].cb;
        hits[nhits].ctx = s_subs[i].ctx;
        nhits++;
    }
    subs_unlock();

    for (size_t i = 0; i < nhits; i++) {
        hits[i].cb(pub->pTopicName, pub->topicNameLength,
                   pub->pPayload, pub->payloadLength, hits[i].ctx);
    }

    if (nhits == 0) {
        /* Not an error: AWS re-delivers on a resumed session, and a filter may
         * have been dropped between SUBSCRIBE and delivery. Worth seeing when a
         * subscription silently does nothing, though. */
        ESP_LOGD(ZC_AWS_TAG, "no subscriber for \"%.*s\"",
                 (int)pub->topicNameLength, pub->pTopicName);
    }
}
