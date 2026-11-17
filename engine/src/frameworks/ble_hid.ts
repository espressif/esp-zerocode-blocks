// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

import type { RenderedInstance } from '../types.js'
import { collectSlot } from '../generator-common.js'
import type { FrameworkGlue, GeneratedFile } from './glue.js'

/**
 * app_ble_hid.cpp — the BLE media-remote framework.
 *
 * The binding inverts the other way: not "how does this device appear" but
 * "what does this device SEND". Controller device types map param-bus
 * changes to HID consumer usages via ble_hid_driver_cb_cases, using
 * zc_ble_hid_send_consumer(usage) (press + release, report id 1).
 *
 * GAP/advertising/security boilerplate lives in the component's
 * ble_hid_gap.c (BLE-only NimBLE, adapted from the IDF example) — this
 * generated file owns the report map, the esp_hidd device, and dispatch.
 */
export function genAppBleHidCpp(rendered: RenderedInstance[]): string {
  // A composed device_types/gamepad adds one gamepad report (id 3) to the map,
  // the gamepad appearance and its own name; without it the output is the
  // media remote's, byte for byte.
  const pad = rendered.find(r => r.block.id === 'device_types/gamepad')
  const padName = pad ? String(pad.cfg.device_name ?? 'ZeroCode Gamepad').replace(/[\\"]/g, '') : ''
  const padMap = pad ? `
    0x05, 0x01,       /* Usage Page (Generic Desktop) */
    0x09, 0x05,       /* Usage (Game Pad) */
    0xA1, 0x01,       /* Collection (Application) */
    0x85, 0x03,       /*   Report ID (3) */
    0x05, 0x09,       /*   Usage Page (Button) */
    0x19, 0x01,       /*   Usage Minimum (Button 1) */
    0x29, 0x10,       /*   Usage Maximum (Button 16) */
    0x15, 0x00,       /*   Logical Minimum (0) */
    0x25, 0x01,       /*   Logical Maximum (1) */
    0x75, 0x01,       /*   Report Size (1) */
    0x95, 0x10,       /*   Report Count (16) */
    0x81, 0x02,       /*   Input (Data, Var, Abs) — buttons */
    0x05, 0x01,       /*   Usage Page (Generic Desktop) */
    0x09, 0x30,       /*   Usage (X) */
    0x09, 0x31,       /*   Usage (Y) */
    0x09, 0x32,       /*   Usage (Z) */
    0x09, 0x35,       /*   Usage (Rz) */
    0x15, 0x81,       /*   Logical Minimum (-127) */
    0x25, 0x7F,       /*   Logical Maximum (127) */
    0x75, 0x08,       /*   Report Size (8) */
    0x95, 0x04,       /*   Report Count (4) */
    0x81, 0x02,       /*   Input (Data, Var, Abs) — two sticks */
    0xC0,             /* End Collection */` : ''
  const padSend = pad ? `
/* The gamepad's whole state, report id 3: button n is bit n-1 of buttons,
 * and the two sticks' axes -127..127 (0 = centred). A state, not a press:
 * send it when anything changed; it is dropped while no host is connected. */
void zc_ble_hid_send_gamepad(uint16_t buttons, int8_t x, int8_t y, int8_t z, int8_t rz)
{
    if (!s_host_connected || s_hid_dev == NULL) {
        ESP_LOGD(TAG, "no host connected — dropping gamepad state");
        return;
    }
    uint8_t report[6] = { (uint8_t)(buttons & 0xff), (uint8_t)(buttons >> 8),
                          (uint8_t)x, (uint8_t)y, (uint8_t)z, (uint8_t)rz };
    esp_hidd_dev_input_set(s_hid_dev, 0, 3, report, sizeof(report));
}
` : ''
  const appearance = pad ? '0x03C4 /* gamepad */' : '0x03C0 /* generic HID */'
  const devName = pad ? padName : 'ZeroCode Remote'
  return `#include "app_ble_hid.h"
#include "ble_hid_gap.h"
#include "app_driver.h"
#include "app_config.h"

#include <string.h>
#include <esp_log.h>
#include <esp_event.h>
#include <esp_hidd.h>
${collectSlot(rendered, 'ble_hid_includes')}

static const char *TAG = "app_ble_hid";

static app_driver_handle_t s_handle = 0;
static esp_hidd_dev_t *s_hid_dev = NULL;
static volatile bool s_host_connected = false;

/* Consumer Control, report id 1: one 16-bit usage array. Press = usage,
 * release = 0. Covers play/pause, volume, mute, next/prev — everything a
 * media remote sends. Keyboard, report id 2: the standard 8-byte input
 * (modifiers, reserved, six keycodes) — arrows, page up/down, space, letters:
 * a presentation clicker, a page turner. */
static const uint8_t s_report_map[] = {
    0x05, 0x0C,       /* Usage Page (Consumer) */
    0x09, 0x01,       /* Usage (Consumer Control) */
    0xA1, 0x01,       /* Collection (Application) */
    0x85, 0x01,       /*   Report ID (1) */
    0x15, 0x00,       /*   Logical Minimum (0) */
    0x26, 0xFF, 0x03, /*   Logical Maximum (0x3FF) */
    0x19, 0x00,       /*   Usage Minimum (0) */
    0x2A, 0xFF, 0x03, /*   Usage Maximum (0x3FF) */
    0x75, 0x10,       /*   Report Size (16) */
    0x95, 0x01,       /*   Report Count (1) */
    0x81, 0x00,       /*   Input (Data, Array) */
    0xC0,             /* End Collection */
    0x05, 0x01,       /* Usage Page (Generic Desktop) */
    0x09, 0x06,       /* Usage (Keyboard) */
    0xA1, 0x01,       /* Collection (Application) */
    0x85, 0x02,       /*   Report ID (2) */
    0x05, 0x07,       /*   Usage Page (Keyboard/Keypad) */
    0x19, 0xE0,       /*   Usage Minimum (Left Control) */
    0x29, 0xE7,       /*   Usage Maximum (Right GUI) */
    0x15, 0x00,       /*   Logical Minimum (0) */
    0x25, 0x01,       /*   Logical Maximum (1) */
    0x75, 0x01,       /*   Report Size (1) */
    0x95, 0x08,       /*   Report Count (8) */
    0x81, 0x02,       /*   Input (Data, Var, Abs) — modifiers */
    0x75, 0x08,       /*   Report Size (8) */
    0x95, 0x01,       /*   Report Count (1) */
    0x81, 0x01,       /*   Input (Const) — reserved */
    0x75, 0x08,       /*   Report Size (8) */
    0x95, 0x06,       /*   Report Count (6) */
    0x15, 0x00,       /*   Logical Minimum (0) */
    0x26, 0xFF, 0x00, /*   Logical Maximum (255): two bytes, as one byte 0xFF is -1 */
    0x19, 0x00,       /*   Usage Minimum (0) */
    0x29, 0xFF,       /*   Usage Maximum (255) */
    0x81, 0x00,       /*   Input (Data, Array) — keys */
    0xC0,             /* End Collection */${padMap}
};

${collectSlot(rendered, 'ble_hid_statics')}

/* Press + release. Dropped (with a log) when no host is connected — a
 * remote pressed before pairing is a no-op, not an error. */
void zc_ble_hid_send_consumer(uint16_t usage)
{
    if (!s_host_connected || s_hid_dev == NULL) {
        ESP_LOGD(TAG, "no host connected — dropping usage 0x%03x", usage);
        return;
    }
    uint8_t report[2] = { (uint8_t)(usage & 0xff), (uint8_t)(usage >> 8) };
    esp_hidd_dev_input_set(s_hid_dev, 0, 1, report, 2);
    uint8_t release[2] = { 0, 0 };
    esp_hidd_dev_input_set(s_hid_dev, 0, 1, release, 2);
}

/* A keyboard key, press + release: a USB HID Keyboard Page keycode (0x4F right
 * arrow, 0x50 left, 0x4B page up, 0x4E page down, 0x2C space) with modifier
 * bits (0x02 left shift). Dropped when no host is connected, like a consumer
 * usage. */
void zc_ble_hid_send_key(uint8_t keycode, uint8_t modifiers)
{
    if (!s_host_connected || s_hid_dev == NULL) {
        ESP_LOGD(TAG, "no host connected — dropping key 0x%02x", keycode);
        return;
    }
    uint8_t report[8] = { modifiers, 0, keycode, 0, 0, 0, 0, 0 };
    esp_hidd_dev_input_set(s_hid_dev, 0, 2, report, 8);
    uint8_t release[8] = { 0 };
    esp_hidd_dev_input_set(s_hid_dev, 0, 2, release, 8);
}

${padSend}/* Hardware/param bus -> HID reports. Device types own the cases. */
static void ble_hid_driver_cb(
    app_driver_param_id_t param_id,
    app_driver_param_val_t val,
    app_driver_handle_t source,
    void *ctx)
{
    (void)source;
    (void)ctx;
    (void)val;
    switch (param_id) {
${collectSlot(rendered, 'ble_hid_driver_cb_cases', 4)}
    default:
        break;
    }
}

static void hidd_event_callback(void *args, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)args; (void)base;
    esp_hidd_event_t event = (esp_hidd_event_t)id;
    esp_hidd_event_data_t *param = (esp_hidd_event_data_t *)event_data;
    switch (event) {
        case ESP_HIDD_START_EVENT:
            ESP_LOGI(TAG, "HID device started — advertising");
            ble_hid_gap_adv_start();
            break;
        case ESP_HIDD_CONNECT_EVENT:
            s_host_connected = true;
            ESP_LOGI(TAG, "HID host connected");
            break;
        case ESP_HIDD_DISCONNECT_EVENT:
            s_host_connected = false;
            ESP_LOGI(TAG, "HID host disconnected");
            break;
        case ESP_HIDD_PROTOCOL_MODE_EVENT:
            ESP_LOGI(TAG, "protocol mode: %s", param->protocol_mode.protocol_mode ? "report" : "boot");
            break;
        default:
            break;
    }
}

esp_err_t app_ble_hid_init(void)
{
    s_handle = app_driver_register_solution("ble_hid", ble_hid_driver_cb, NULL);

    ESP_ERROR_CHECK(ble_hid_gap_stack_init());
    ESP_ERROR_CHECK(ble_hid_gap_adv_init(${appearance}, "${devName}"));

    static esp_hid_raw_report_map_t report_maps[] = {
        { .data = s_report_map, .len = sizeof(s_report_map) },
    };
    static esp_hid_device_config_t hid_config = {};
    hid_config.vendor_id = 0x16A8;
    hid_config.product_id = 0x2C0D;
    hid_config.version = 0x0100;
    hid_config.device_name = "${devName}";
    hid_config.manufacturer_name = "ZeroCode AI";
    hid_config.serial_number = "0";
    hid_config.report_maps = report_maps;
    hid_config.report_maps_len = 1;
    ESP_ERROR_CHECK(esp_hidd_dev_init(&hid_config, ESP_HID_TRANSPORT_BLE,
                                      hidd_event_callback, &s_hid_dev));

    /* Node-level contributions (behaviors) before the host task starts. */
${collectSlot(rendered, 'ble_hid_node_init', 4)}

    ESP_ERROR_CHECK(ble_hid_gap_start_host());

    ESP_LOGI(TAG, "BLE HID initialized — pair from your phone/PC Bluetooth settings");
    return ESP_OK;
}
`
}

export const bleHidGlue: FrameworkGlue = {
  id: 'ble_hid',
  files(ctx) {
    const { rendered, hasDeviceTypes } = ctx
    const out: GeneratedFile[] = []
    out.push({ path: 'components/app_ble_hid/app_ble_hid.cpp', content: genAppBleHidCpp(rendered) })
    if (hasDeviceTypes) {
      out.push({ path: 'components/app_ble_hid/CMakeLists.txt', content: `idf_component_register(
    SRCS "app_ble_hid.cpp" "ble_hid_gap.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_hid bt esp_event nvs_flash app_driver app_config
)
` })
    }
    return out
  },
}
