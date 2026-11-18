// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

import type { RenderedInstance } from '../types.js'
import { collectSlot, indentText, unwrapInitBody } from '../generator-common.js'
import type { FrameworkGlue, GeneratedFile } from './glue.js'

/**
 * app_display.cpp — the LVGL touchscreen UI framework.
 *
 * Inverted binding like audio's: device types contribute a CARD (widgets +
 * event wiring) via display_widget_create, and param changes flow back into
 * widgets via display_driver_cb_cases.
 *
 * Lock discipline is centralized: the whole driver_cb case switch runs under
 * lvgl_port_lock (param-bus notifications arrive off the LVGL task). Widget
 * event callbacks run ON the LVGL task and must not lock. Slot code never
 * locks — a nested lock would deadlock.
 *
 * The panel is an instance block: its display_node_init creates the LVGL
 * display and assigns s_disp (framework blocks carry no cfg, so pins cannot
 * live here). A missing panel block is a loud init failure, not a blank
 * screen.
 */
export function genAppDisplayCpp(rendered: RenderedInstance[]): string {
  const usedCard = new Set<string>()
  const cardFns: string[] = []
  const cardCalls: string[] = []
  for (const r of rendered) {
    if (!(r.slots['display_widget_create'] ?? '').trim()) continue
    const slug = (r.prefix || r.block.id.split('/').pop() || 'card').toLowerCase().replace(/[^a-z0-9_]/g, '_')
    let name = slug
    for (let n = 2; usedCard.has(name); n++) name = `${slug}_${n}`
    usedCard.add(name)
    cardFns.push(`static void zc_${name}_display_card_init(lv_obj_t *parent)\n{\n${indentText(unwrapInitBody(r.slots['display_widget_create']), 4)}\n}`)
    cardCalls.push(`    zc_${name}_display_card_init(col);`)
  }
  return `#include "app_display.h"
#include "app_driver.h"
#include "app_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <esp_log.h>
// The emitted display_setup_task uses the FreeRTOS task API directly, so this
// file must include it itself. It used to arrive transitively via
// esp_lvgl_port.h, which is exactly the include-by-luck that broke seven
// sensor blocks when driver/i2c.h went away — a panel whose own includes do
// not pull FreeRTOS (the MIPI EK79007 does not) then fails on xTaskCreate.
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lvgl.h>
#include <esp_lvgl_port.h>
${collectSlot(rendered, 'display_includes')}

static const char *TAG = "app_display";

static app_driver_handle_t s_handle = 0;

/* Assigned by the panel driver block's display_node_init slot. */
static lv_display_t *s_disp = NULL;

/* Weak: product code defines app_display_on_touch() (app_display.h) to act on
 * any touch — wake a dimmed backlight, switch views. */
extern "C" __attribute__((weak)) void app_display_on_touch(void) {}

static void zc_display_touch_cb(lv_event_t *e)
{
    (void)e;
    app_display_on_touch();
}

${collectSlot(rendered, 'display_statics')}

/* Hardware/param bus -> widgets. Runs OFF the LVGL task, so the whole
 * dispatch holds the LVGL mutex (0 = wait forever). Slot cases must NOT
 * lock again. */
static void display_driver_cb(
    app_driver_param_id_t param_id,
    app_driver_param_val_t val,
    app_driver_handle_t source,
    void *ctx)
{
    (void)source;
    (void)ctx;
    (void)val;
    if (!lvgl_port_lock(0)) return;
    switch (param_id) {
${collectSlot(rendered, 'display_driver_cb_cases', 4)}
    default:
        break;
    }
    lvgl_port_unlock();
}

${cardFns.join('\n\n')}

/* Panel/touch bring-up + widget tree, in a task so it can NEVER block boot:
 * SPI panel init hangs forever on the emulator (and can stall on real
 * miswired hardware) — boot must still reach "ZeroCode AI firmware started"
 * or functional_testing times out on healthy firmware. Same degraded-state
 * philosophy as the camera driver. */
static void display_setup_task(void *arg)
{
    (void)arg;
    /* LVGL port init lives HERE too — under the emulator it never returns
     * (no SPI/LCD to drive), and it must not take app_main down with it. */
    const lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    if (lvgl_port_init(&port_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "lvgl_port_init failed — display disabled");
        vTaskDelete(NULL);
        return;
    }

    /* Panel (and optional touch) hardware — instance blocks, in product.yml
     * order. The panel's slot assigns s_disp. */
${collectSlot(rendered, 'display_node_init', 4)}

    if (s_disp == NULL) {
        ESP_LOGE(TAG, "no display panel block in this product — add drivers/display_ili9341_spi");
        vTaskDelete(NULL);
        return;
    }

    /* Every touch, wherever it lands, reaches app_display_on_touch(): the
     * hook sits on the input device, which LVGL tells before any object, so a
     * full-screen object of the product's own cannot swallow it. */
    if (lvgl_port_lock(0)) {
        for (lv_indev_t *in = lv_indev_get_next(NULL); in != NULL; in = lv_indev_get_next(in)) {
            if (lv_indev_get_type(in) == LV_INDEV_TYPE_POINTER) {
                lv_indev_add_event_cb(in, zc_display_touch_cb, LV_EVENT_PRESSED, NULL);
            }
        }
        lvgl_port_unlock();
    }

${cardCalls.length ? `    /* One scrollable column of device cards, built under the LVGL lock. */
    if (lvgl_port_lock(0)) {
        lv_obj_t *col = lv_obj_create(lv_display_get_screen_active(s_disp));
        lv_obj_set_size(col, lv_pct(100), lv_pct(100));
        lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(col, 8, 0);
        /* This container is full-bleed, so EVERY touch lands on it or a card —
         * never on the screen. Without bubbling, a screen-level input handler
         * (the obvious place to hook backlight wake, screensaver dismiss, an
         * activity timer) is registered successfully and then never fires.
         * That is silent and survives review: a panel that dims after a few
         * minutes then never wakes on a touch. Bubble input events up so the
         * screen really does see them. */
        lv_obj_add_flag(col, LV_OBJ_FLAG_EVENT_BUBBLE);
${cardCalls.join('\n')}
        lvgl_port_unlock();
    }
` : `    /* No device cards: the screen is the product's own. A full-screen column
     * here (opaque under LVGL's light theme) would cover a product that draws
     * its own UI. */
`}
    /* Register only once widgets exist — driver_cb cases null-check anyway. */
    s_handle = app_driver_register_solution("display", display_driver_cb, NULL);

    ESP_LOGI(TAG, "Display initialized");
    vTaskDelete(NULL);
}

esp_err_t app_display_init(void)
{
    xTaskCreate(display_setup_task, "display_setup", 6144, NULL, 4, NULL);
    ESP_LOGI(TAG, "Display setup started");
    return ESP_OK;
}
`
}

export const displayGlue: FrameworkGlue = {
  id: 'display',
  files(ctx) {
    const { rendered, hasDeviceTypes, product, boardExtras, bmgrBoard, boardFacts } = ctx
    const out: GeneratedFile[] = []
    // The BOARD goes first. It is a panel contributor like any driver block
    // (see boardRendered), and the panel must bind before the touch that
    // registers against it — ordering here is the same rule product.yml
    // already has, applied to the one contributor that is not in product.yml.
    const displayRendered = [...boardExtras, ...rendered]
    if (!displayRendered.some(r => (r.slots['display_node_init'] ?? '').trim())) {
      // Loud at GENERATION, because the alternative is a display product whose
      // only symptom is a dark screen. A boardless product still reaches the
      // framework's own runtime check; a board product does not get to
      // discover on the bench that its pack was not readable.
      throw new Error(
        `product '${product.id}' uses the display framework but nothing provides a panel` +
        (bmgrBoard
          ? `. Board '${bmgrBoard}' declares no display_lcd device` +
            (boardFacts ? '' : ` and its pack could not be read (set ZC_BOARDS_DIR)`) +
            `, and the product instantiates no panel driver block.`
          : ` — add a panel driver block (e.g. drivers/display_ili9341_spi).`),
      )
    }
    out.push({ path: 'components/app_display/app_display.cpp', content: genAppDisplayCpp(displayRendered) })
    if (hasDeviceTypes) {
      out.push({ path: 'components/app_display/CMakeLists.txt', content: `idf_component_register(
    SRCS "app_display.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES lvgl__lvgl espressif__esp_lvgl_port esp_lcd app_driver app_config
)
` })
    }
    return out
  },
}
