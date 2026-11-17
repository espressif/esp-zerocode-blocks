// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

import type { RenderedInstance } from '../types.js'
import { collectSlot } from '../generator-common.js'
import type { FrameworkGlue, GeneratedFile } from './glue.js'

/**
 * app_vision.cpp — the camera perception framework.
 *
 * The lightest framework spine: detector DRIVER blocks (camera + algorithm)
 * own the hardware and their capture/detect tasks, and report through
 * zc_vision_emit(event_id, value); this file dispatches those events into
 * device-type / behavior slots (vision_event_cases) which drive the param
 * bus. Detection stays in blocks so new detectors (esp-who person/face)
 * extend the event vocabulary without generator changes.
 */
export function genAppVisionCpp(rendered: RenderedInstance[]): string {
  return `#include "app_vision.h"
#include "app_driver.h"
#include "app_config.h"

#include <esp_log.h>
${collectSlot(rendered, 'vision_includes')}

static const char *TAG = "app_vision";

static app_driver_handle_t s_handle = 0;

${collectSlot(rendered, 'vision_statics')}

/* Detector blocks report here (from their own tasks). Slots see \`event_id\`
 * and \`value\`. Vocabulary: 0 = motion (0/1); detector blocks extend it. */
void zc_vision_emit(int event_id, int value)
{
    (void)event_id;
    (void)value;
${collectSlot(rendered, 'vision_event_cases', 4)}
}

esp_err_t app_vision_init(void)
{
    s_handle = app_driver_register_solution("vision", NULL, NULL);

    /* Camera + detector hardware — instance blocks, in product.yml order. */
${collectSlot(rendered, 'vision_node_init', 4)}

    ESP_LOGI(TAG, "Vision initialized");
    return ESP_OK;
}
`
}

export const visionGlue: FrameworkGlue = {
  id: 'vision',
  files(ctx) {
    const { rendered, hasDeviceTypes } = ctx
    const out: GeneratedFile[] = []
    out.push({ path: 'components/app_vision/app_vision.cpp', content: genAppVisionCpp(rendered) })
    if (hasDeviceTypes) {
      out.push({ path: 'components/app_vision/CMakeLists.txt', content: `idf_component_register(
    SRCS "app_vision.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES app_driver app_config
)
` })
    }
    return out
  },
}
