// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

import type { RenderedInstance } from '../types.js'
import { collectSlot } from '../generator-common.js'
import type { FrameworkGlue, GeneratedFile } from './glue.js'

/**
 * app_sound.cpp — the Audio framework (id: sound): the device PLAYS sound.
 * A pattern sequencer over the param bus driving the product's existing
 * tone driver (any block with a tone_param) — no second I2S owner; the speaker instance's
 * cfg.tone_param is baked in at generation. No speaker instance = the
 * framework compiles to a logged no-op (a silent product is a valid dev
 * state; a link error is not).
 */
export function genAppSoundCpp(rendered: RenderedInstance[]): string {
  // The speaker is whichever instance implements the tone contract — its block
  // declares a `tone_param` driver param (speaker_i2s, speaker_es8311,
  // board_audio_codec). Matching one block id left every other speaker silent
  // under the sound framework with nothing but a boot warning to say so.
  const speaker =
    rendered.find(r => r.block.id === 'drivers/speaker_i2s') ??
    rendered.find(r => {
      // assemble_blocks folds driver_params into params and records their types
      // under param_refs; a block read straight from source still has driver_params.
      const b = r.block as { param_refs?: Record<string, unknown>; driver_params?: Record<string, unknown> }
      return !!(b.param_refs?.tone_param ?? b.driver_params?.tone_param) && !!(r.cfg as Record<string, unknown>)?.tone_param
    })
  const toneParam = speaker ? String((speaker.cfg as Record<string, unknown>)?.tone_param ?? '') : ''
  const body = '#include "app_sound.h"\n#include "app_driver.h"\n#include "app_config.h"\n\n#include <string.h>\n#include <esp_log.h>\n#include <freertos/FreeRTOS.h>\n#include <freertos/task.h>\n__SLOT_INCLUDES__\n\nstatic const char *TAG = "app_sound";\n\n#define ZC_SOUND_MAX_NOTES 24\nstatic zc_tone_t s_pattern[ZC_SOUND_MAX_NOTES];\nstatic volatile size_t s_count = 0;\nstatic volatile uint32_t s_generation = 0; /* bump = restart with new pattern */\nstatic TaskHandle_t s_task = NULL;\nstatic app_driver_handle_t s_handle = 0;\n\nesp_err_t app_sound_play(const zc_tone_t *pattern, size_t count)\n{\n__NO_SPEAKER_EARLY__\n    if (pattern == NULL || count == 0) return ESP_ERR_INVALID_ARG;\n    if (count > ZC_SOUND_MAX_NOTES) count = ZC_SOUND_MAX_NOTES;\n    /* Latest wins: alerts must not queue behind stale chimes. */\n    memcpy(s_pattern, pattern, count * sizeof(zc_tone_t));\n    s_count = count;\n    s_generation = s_generation + 1;\n    if (s_task) xTaskNotifyGive(s_task);\n    return ESP_OK;\n}\n\n__PLAYER_BODY__\n\n/* Device-type sound reactions ("what does this device sound like"). */\nstatic void sound_driver_cb(app_driver_param_id_t param_id, app_driver_param_val_t val, app_driver_handle_t source, void *arg)\n{\n    (void)source;\n    switch (param_id) {\n__SLOT_EVENT_CASES__\n        default: return;\n    }\n}\n\nesp_err_t app_sound_init(void)\n{\n    s_handle = app_driver_register_solution("sound", sound_driver_cb, NULL);\n__PLAYER_START__\n    ESP_LOGI(TAG, "Audio (sound) framework ready");\n    return ESP_OK;\n}'
  if (!toneParam) {
    return body
      .replace('__SLOT_INCLUDES__', collectSlot(rendered, 'sound_includes'))
      .replace('__NO_SPEAKER_EARLY__', '    static bool warned = false;\n    if (!warned) { warned = true; ESP_LOGW(TAG, "no speaker instance — sound is a no-op"); }\n    (void)pattern; (void)count;\n    return ESP_OK;')
      .replace('__PLAYER_BODY__', '/* No speaker instance: no sequencer. */')
      .replace('__PLAYER_START__', '')
      .replace('__SLOT_EVENT_CASES__', collectSlot(rendered, 'sound_event_cases', 8))
  }
  return body
    .replace('__SLOT_INCLUDES__', collectSlot(rendered, 'sound_includes'))
    .replace('__NO_SPEAKER_EARLY__', '')
    .replace('__PLAYER_BODY__', "/* Sequencer: steps the pattern into the speaker driver via its tone param\n * (__TONE_PARAM__ — baked from the product's speaker_i2s instance). The\n * driver owns I2S, volume and mute; this task only writes frequencies. */\nstatic void sound_player_task(void *arg)\n{\n    for (;;) {\n        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);\n        uint32_t gen = s_generation;\n        size_t n = s_count;\n        for (size_t i = 0; i < n && gen == s_generation; i++) {\n            app_driver_param_val_t v = { .u32 = s_pattern[i].freq_hz };\n            app_driver_set_param(__TONE_PARAM__, v, s_handle);\n            vTaskDelay(pdMS_TO_TICKS(s_pattern[i].ms ? s_pattern[i].ms : 1));\n        }\n        if (gen == s_generation) {\n            app_driver_param_val_t off = { .u32 = 0 };\n            app_driver_set_param(__TONE_PARAM__, off, s_handle);\n        }\n    }\n}".split('__TONE_PARAM__').join(toneParam))
    .replace('__PLAYER_START__', '    if (xTaskCreate(sound_player_task, "zc_sound", 3072, NULL, 5, &s_task) != pdPASS) {\n        ESP_LOGE(TAG, "player task create failed");\n        return ESP_FAIL;\n    }')
    .replace('__SLOT_EVENT_CASES__', collectSlot(rendered, 'sound_event_cases', 8))
}

export const soundGlue: FrameworkGlue = {
  id: 'sound',
  files(ctx) {
    const { rendered } = ctx
    const out: GeneratedFile[] = []
    out.push({ path: 'components/app_sound/app_sound.cpp', content: genAppSoundCpp(rendered) })
    out.push({ path: 'components/app_sound/CMakeLists.txt', content: `idf_component_register(
    SRCS "app_sound.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES app_driver app_config
)
` })
    return out
  },
}
