// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

import type { RenderedInstance } from '../types.js'
import { collectSlot } from '../generator-common.js'
import type { FrameworkGlue, GeneratedFile } from './glue.js'

/**
 * app_audio.cpp — the esp-sr voice framework.
 *
 * Structurally the sibling of genAppMatterCpp/genAppRainmakerCpp/genAppZigbeeCpp,
 * but the binding is inverted: device types contribute PHRASES rather than
 * endpoints, and a recognized phrase id drives app_driver_set_param.
 *
 * Two tasks by necessity, not preference: AFE's feed and fetch halves must run
 * concurrently (feed blocks on I2S, fetch blocks on AFE's internal queue), so
 * running them on one task deadlocks as soon as either side stalls.
 */
export function genAppAudioCpp(rendered: RenderedInstance[]): string {
  // The weak default only when no block defines the real one: a microphone
  // block's audio_statics land in THIS file, and a strong and a weak definition
  // in one translation unit is a redefinition error, not an override.
  const micProvided = /\bzc_audio_mic_read\s*\([^;{)]*\)\s*\{/.test(collectSlot(rendered, 'audio_statics'))
  const micDefault = micProvided ? '' : `/* Silence until a microphone block provides the real zc_audio_mic_read. */
__attribute__((weak)) size_t zc_audio_mic_read(int16_t *dest, size_t samples)
{
    (void)dest;
    (void)samples;
    return 0;
}

`
  return `#include "app_audio.h"
#include "app_driver.h"
#include "app_config.h"

#include <esp_log.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_afe_config.h>
#include <esp_afe_sr_iface.h>
#include <esp_afe_sr_models.h>
#include <esp_mn_iface.h>
#include <esp_mn_models.h>
#include <esp_mn_speech_commands.h>
#include <model_path.h>
${collectSlot(rendered, 'audio_includes')}

static const char *TAG = "app_audio";

static app_driver_handle_t s_handle = 0;

static srmodel_list_t *s_models = NULL;
static const esp_afe_sr_iface_t *s_afe = NULL;
static esp_afe_sr_data_t *s_afe_data = NULL;
static const esp_mn_iface_t *s_mn = NULL;
static model_iface_data_t *s_mn_data = NULL;

${collectSlot(rendered, 'audio_statics')}

/* Recognized command -> hardware. Device types own the cases, keyed by the
 * command id each phrase was registered with (esp_mn_commands_add) — not
 * MultiNet's phrase_id, which is the phrase's index in its list and differs
 * once a second device or a synonym phrase is registered. */
static void audio_apply_command(int command_id)
{
    switch (command_id) {
${collectSlot(rendered, 'audio_command_cases', 4)}
    default:
        break;
    }
    /* Reached only when no case claimed the phrase — every device-type case
     * returns. A phrase registered with no matching case means the block's
     * audio_commands_register and audio_command_cases disagree. */
    ESP_LOGW(TAG, "command %d recognized but no device type claims it", command_id);
}

/* Listening-state transitions. Behavior blocks hook these via
 * audio_event_cases — \`listening\` is in scope: 1 = wake word detected,
 * command window open; 0 = window closed (command handled or timed out).
 * The audio analog of matter_event_cases / zigbee_signal_cases. */
static void audio_on_event(int listening)
{
    (void)listening;
${collectSlot(rendered, 'audio_event_cases', 4)}
}

/* Hardware/param bus -> voice feedback. Device types may announce state here. */
static void audio_driver_cb(
    app_driver_param_id_t param_id,
    app_driver_param_val_t val,
    app_driver_handle_t source,
    void *ctx)
{
    (void)param_id;
    (void)val;
    (void)source;
    (void)ctx;
${collectSlot(rendered, 'audio_driver_cb_cases', 4)}
}

${micDefault}static void audio_feed_task(void *arg)
{
    (void)arg;
    const int chunk = s_afe->get_feed_chunksize(s_afe_data);
    const int channels = s_afe->get_feed_channel_num(s_afe_data);
    int16_t *buf = (int16_t *)heap_caps_malloc(chunk * channels * sizeof(int16_t),
                                               MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) {
        ESP_LOGE(TAG, "feed buffer alloc failed (%d samples)", chunk * channels);
        vTaskDelete(NULL);
        return;
    }
    while (1) {
        /* Provided by drivers/voice_mic_i2s (fixed-name contract, app_audio.h). */
        if (zc_audio_mic_read(buf, chunk * channels) == 0) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        s_afe->feed(s_afe_data, buf);
    }
}

static void audio_detect_task(void *arg)
{
    (void)arg;
    /* Wake word gates command recognition: MultiNet only runs in the window
     * after a detected wake word, which is what keeps idle CPU down. */
    bool listening = false;

    while (1) {
        afe_fetch_result_t *res = s_afe->fetch(s_afe_data);
        if (!res || res->ret_value == ESP_FAIL) continue;

        if (!listening && res->wakeup_state == WAKENET_DETECTED) {
            listening = true;
            s_mn->clean(s_mn_data);
            ESP_LOGI(TAG, "wake word detected — listening for a command");
            audio_on_event(1);
            continue;
        }

        if (!listening) continue;

        esp_mn_state_t st = s_mn->detect(s_mn_data, res->data);
        if (st == ESP_MN_STATE_DETECTING) continue;

        if (st == ESP_MN_STATE_DETECTED) {
            esp_mn_results_t *mn = s_mn->get_results(s_mn_data);
            if (mn && mn->num > 0) {
                ESP_LOGI(TAG, "command %d (phrase %d)", mn->command_id[0], mn->phrase_id[0]);
                audio_apply_command(mn->command_id[0]);
            }
        } else if (st == ESP_MN_STATE_TIMEOUT) {
            ESP_LOGI(TAG, "no command heard — sleeping until next wake word");
        }
        listening = false;
        audio_on_event(0);
    }
}

esp_err_t app_audio_init(void)
{
    s_handle = app_driver_register_solution("audio", audio_driver_cb, NULL);

    /* Capture hardware first — AFE has nothing to read without it. */
${collectSlot(rendered, 'audio_node_init', 4)}

    /* Models live in the \`model\` partition; see the 8mb-voice table. A build
     * with no model partition reaches here and fails, which is the intended
     * loud failure rather than a silent mis-detection. */
    s_models = esp_srmodel_init("model");
    if (!s_models || s_models->num <= 0) {
        ESP_LOGE(TAG, "no esp-sr models found — is the 'model' partition flashed?");
        return ESP_ERR_NOT_FOUND;
    }

    char *wn_name = esp_srmodel_filter(s_models, ESP_WN_PREFIX, NULL);
    char *mn_name = esp_srmodel_filter(s_models, ESP_MN_PREFIX, ESP_MN_ENGLISH);
    if (!wn_name || !mn_name) {
        ESP_LOGE(TAG, "missing model (wakenet=%s multinet=%s)",
                 wn_name ? wn_name : "none", mn_name ? mn_name : "none");
        return ESP_ERR_NOT_FOUND;
    }
    ESP_LOGI(TAG, "models: wakenet=%s multinet=%s", wn_name, mn_name);

    /* "M" = one mic channel, no reference channel (no AEC — nothing plays back
     * into this mic in the reference products). */
    afe_config_t *afe_cfg = afe_config_init("M", s_models, AFE_TYPE_SR, AFE_MODE_LOW_COST);
    if (!afe_cfg) {
        ESP_LOGE(TAG, "afe_config_init failed");
        return ESP_ERR_NO_MEM;
    }
    s_afe = esp_afe_handle_from_config(afe_cfg);
    s_afe_data = s_afe->create_from_config(afe_cfg);
    afe_config_free(afe_cfg);
    if (!s_afe_data) {
        ESP_LOGE(TAG, "AFE create failed");
        return ESP_ERR_NO_MEM;
    }

    /* 6000 ms command window after the wake word. */
    s_mn = esp_mn_handle_from_name(mn_name);
    s_mn_data = s_mn->create(mn_name, 6000);
    if (!s_mn_data) {
        ESP_LOGE(TAG, "MultiNet create failed");
        return ESP_ERR_NO_MEM;
    }

    /* Phrases are registered at runtime, not from sdkconfig: the vocabulary is
     * composed from whichever device types the product includes. */
    esp_mn_commands_clear();
${collectSlot(rendered, 'audio_commands_register', 4)}
    esp_mn_commands_update();
    s_mn->print_active_speech_commands(s_mn_data);

    xTaskCreate(audio_feed_task, "audio_feed", 4096, NULL, 5, NULL);
    xTaskCreate(audio_detect_task, "audio_detect", 8192, NULL, 5, NULL);

    ESP_LOGI(TAG, "Voice framework initialized");
    return ESP_OK;
}
`
}

export const audioGlue: FrameworkGlue = {
  id: 'audio',
  files(ctx) {
    const { rendered, hasDeviceTypes } = ctx
    const out: GeneratedFile[] = []
    out.push({ path: 'components/app_audio/app_audio.cpp', content: genAppAudioCpp(rendered) })
    if (hasDeviceTypes) {
      out.push({ path: 'components/app_audio/CMakeLists.txt', content: `idf_component_register(
    SRCS "app_audio.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES espressif__esp-sr app_driver app_config esp_driver_i2s
)
` })
    }
    return out
  },
}
