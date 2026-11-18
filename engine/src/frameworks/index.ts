// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// Every framework that generates its own glue, in the order generate() writes
// them. A framework missing here (aws_iot, lm, tflite) ships only its block's
// components. A new one is a module beside these and a line below.

import type { FrameworkGlue } from './glue.js'
import { matterGlue } from './matter.js'
import { rainmakerGlue } from './rainmaker.js'
import { zigbeeGlue } from './zigbee.js'
import { bleMeshGlue } from './ble_mesh.js'
import { webuiGlue } from './webui.js'
import { espnowGlue } from './espnow.js'
import { bleHidGlue } from './ble_hid.js'
import { visionGlue } from './vision.js'
import { displayGlue } from './display.js'
import { mqttGlue } from './mqtt.js'
import { soundGlue } from './sound.js'
import { agentsGlue } from './agents.js'
import { audioGlue } from './audio.js'

export const FRAMEWORK_GLUE: readonly FrameworkGlue[] = [
  matterGlue,
  rainmakerGlue,
  zigbeeGlue,
  bleMeshGlue,
  webuiGlue,
  espnowGlue,
  bleHidGlue,
  visionGlue,
  displayGlue,
  mqttGlue,
  soundGlue,
  agentsGlue,
  audioGlue,
]
