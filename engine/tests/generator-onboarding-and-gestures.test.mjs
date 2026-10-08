// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

/**
 * Onboarding the product can build on, and the board's own button + LED with
 * the surface a product actually needs. Generated over the REAL catalog, so a
 * block edit that loses any of it fails here.
 *
 * What each test is really guarding (2026-10, from six local runs of one AWS
 * IoT light, every one of which hand-wrote all three):
 *
 *   - console onboarding is a COMMAND THE FRAMEWORK SHIPS (aws-setup /
 *     mqtt-setup) over PUBLIC SETTERS, and the storage exists once. Before,
 *     the setters were static, so "the device asks for its credentials" cost a
 *     430-500 line wizard that copied the NVS layout or edited the framework;
 *   - the shared-network mqtt variant (Matter/RainMaker owns Wi-Fi) keeps
 *     refusing Wi-Fi — its setter says NOT_SUPPORTED and no mqtt-wifi exists;
 *   - drivers/board_button carries gestures (double click, long press, a hold
 *     to factory reset) on the BOARD's handle, and an unchanged product (a
 *     single-click toggle) generates no gesture timing it did not ask for;
 *   - drivers/board_led_strip renders colour only when a hue param is bound,
 *     and stays the on/off fixed-colour indicator otherwise.
 */
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { execFileSync } from 'node:child_process'
import { mkdtempSync, mkdirSync, writeFileSync, readFileSync, rmSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const eng = await import(join(ROOT, 'engine/dist/index.js'))

/** The real catalog, assembled the way the publish step does; plus a board
 *  pack carrying a devkit's two on-board devices. */
function setup(t) {
  const root = mkdtempSync(join(tmpdir(), 'zc-onb-'))
  t.after(() => rmSync(root, { recursive: true, force: true }))
  const tpl = join(root, 'templates')
  execFileSync('python3', [join(ROOT, 'scripts/assemble_blocks.py'),
    '--out', join(tpl, 'code_blocks'), '--products-out', join(tpl, 'product_configurations')], { stdio: 'ignore' })
  const boards = join(root, 'boards')
  const board = join(boards, 'pack', 'demo_c3_devkit')
  mkdirSync(board, { recursive: true })
  writeFileSync(join(board, 'board_info.yaml'), 'board: demo_c3_devkit\nchip: esp32c3\n')
  writeFileSync(join(board, 'sdkconfig.defaults.board'), 'CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y\n')
  writeFileSync(join(board, 'board_devices.yaml'),
    'devices:\n  - name: led_strip\n    type: led_strip\n  - name: boot_button\n    type: button\n')
  const paths = { templatesDir: tpl, baseFirmwareDir: join(ROOT, 'base_firmware'), boardsDir: boards }
  let n = 0
  const gen = async (product, { board: withBoard = false } = {}) => {
    const outDir = join(root, `out-${n++}`)
    await eng.generate(paths, {
      product,
      board: withBoard ? { schema: 'zc-board/1', selected: 'demo', chip: 'esp32c3',
                           bmgr: { board: 'demo_c3_devkit', resolved_from: 'exact' } } : null,
      outDir, chip: 'esp32c3',
    })
    return (rel) => readFileSync(join(outDir, rel), 'utf8')
  }
  const catalog = async (id) => {
    const all = await eng.listAllProducts(paths)
    const entry = all.find((p) => p.product.id === id || p.id === id)
    assert.ok(entry, `${id} is in the catalog`)
    return entry.product
  }
  return { gen, catalog }
}

/** Code only: the slots explain in comments what they deliberately do NOT
 *  call, and a test must not read that as a call. */
const code = (src) => src.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/.*$/gm, '')

const demo = (instances, frameworks = []) => ({ id: 'demo', name: 'demo', description: 'demo', frameworks, instances })

/* ── console onboarding ──────────────────────────────────────────────── */

test('aws_iot ships a guided aws-setup over public setters, and the storage exists once', async (t) => {
  const { gen, catalog } = setup(t)
  const read = await gen(await catalog('aws-iot-conn-demo'))
  const header = read('components/app_aws_iot/include/app_aws_iot.h')
  for (const fn of ['app_aws_iot_set_wifi', 'app_aws_iot_set_endpoint', 'app_aws_iot_set_thing',
                    'app_aws_iot_set_cert', 'app_aws_iot_set_key', 'app_aws_iot_set_root_ca',
                    'app_aws_iot_clear_credentials', 'app_aws_iot_configured', 'app_aws_iot_owns_wifi']) {
    assert.match(header, new RegExp(`\\b${fn}\\(`), `app_aws_iot.h declares ${fn}`)
  }
  // A PEM setter takes NUL-terminated text and no length: strlen() vs
  // strlen()+1 is the mbedTLS mistake a generated product shipped.
  assert.match(header, /esp_err_t app_aws_iot_set_cert\(const char \*pem\);/)
  const console = read('components/app_aws_iot/zc_aws_console.cpp')
  assert.match(console, /"aws-setup"/, 'the guided command is registered')
  assert.match(console, /app_console_read_line\(/, 'it reads answers through the shared reader')
  // ONE implementation of the storage: the commands are wrappers.
  assert.ok(!/nvs_(open|set_|erase)/.test(console), 'no NVS access left in the console commands')
  assert.ok(!/fcntl|read\(STDIN_FILENO/.test(console), 'no private stdin reader left in the framework')
  assert.match(read('components/app_aws_iot/CMakeLists.txt'), /"zc_aws_config\.cpp"/)
  // The last will: a {thing} template substituted at EACH connect (the thing
  // name is not final until onboarding) and passed to MQTT_Connect; plus the
  // retained publish its "online" counterpart needs. Three of three writers
  // that reached it hand-added a will to this component before it existed.
  assert.match(header, /esp_err_t app_aws_iot_set_will\(const char \*topic, const char \*payload, uint8_t qos, bool retain\);/)
  assert.match(header, /esp_err_t app_aws_iot_publish_retained\(/)
  const agent = read('components/app_aws_iot/zc_aws_agent.cpp')
  assert.match(agent, /zc_aws_will_for_connect\(&will, c->thing, will_topic, sizeof\(will_topic\),\s*will_payload, sizeof\(will_payload\)\) \? &will : NULL;\s*\n\s*bool session_present = false;\s*\n\s*st = MQTT_Connect\(&s_agent\.mqttContext, &ci, p_will,/)
  assert.match(read('components/app_aws_iot/zc_aws_config.cpp'), /strncmp\(p, "\{thing\}", 7\) == 0\) \{ ins = thing;/)
  // the heartbeat uses it when presence is on: will set at init, retained "online" on connect
  const withPresence = await gen(demo([
    { block: 'behaviors/aws_iot_heartbeat', prefix: 'HEARTBEAT', cfg: { topic_prefix: 'zerocode', period_s: 30, presence: 1 } },
  ], ['aws_iot']))
  const logic = withPresence('components/app_logic/control.cpp')
  assert.match(logic, /app_aws_iot_set_will\(HEARTBEAT_TOPIC_PREFIX "\/\{thing\}\/status",\s*"offline", 1, true\)/)
  assert.match(logic, /app_aws_iot_publish_retained\(status, "online"/)
  const appConsole = read('components/app_console/include/app_console.h')
  assert.match(appConsole, /esp_err_t app_console_read_line\(/)
  assert.match(appConsole, /esp_err_t app_console_read_text\(const char \*prompt, const app_console_text_opts_t \*opts,/)
})

test('mqtt owning Wi-Fi: setters, wrappers and mqtt-setup', async (t) => {
  const { gen, catalog } = setup(t)
  const read = await gen(await catalog('mqtt-color-light'))
  const src = read('components/app_mqtt/app_mqtt.cpp')
  assert.match(src, /esp_err_t app_mqtt_set_wifi\(const char \*ssid, const char \*password\)\n\{\n    if \(ssid == NULL/)
  assert.match(src, /esp_err_t app_mqtt_set_broker\(/)
  assert.match(src, /\.command = "mqtt-wifi"/)
  assert.match(src, /\.command = "mqtt-setup"/)
  // the commands are wrappers: no NVS write outside the setters
  const wifiCmd = src.slice(src.indexOf('static int mqtt_wifi_cmd'), src.indexOf('static int mqtt_broker_cmd'))
  assert.match(wifiCmd, /app_mqtt_set_wifi\(argv\[1\], argv\[2\]\)/)
  assert.ok(!/nvs_/.test(wifiCmd))
  assert.match(read('components/app_mqtt/include/app_mqtt.h'), /bool app_mqtt_configured\(void\);/)
})

test('mqtt beside Matter: Wi-Fi stays the transport\'s, the setter refuses it, mqtt-setup skips it', async (t) => {
  const { gen, catalog } = setup(t)
  const read = await gen(await catalog('smart-plug-ha'))
  const src = read('components/app_mqtt/app_mqtt.cpp')
  assert.ok(!src.includes('"mqtt-wifi"'), 'no mqtt-wifi command in shared-network mode')
  assert.match(src, /esp_err_t app_mqtt_set_wifi[\s\S]*?return ESP_ERR_NOT_SUPPORTED;/)
  assert.match(src, /\.command = "mqtt-setup"/)
  assert.match(src, /bool app_mqtt_owns_wifi\(void\)\n\{\n    return false;/)
})

/* ── the board's own button: gestures ────────────────────────────────── */

test('board_button: double click, long press and a factory-reset hold, on the board handle', async (t) => {
  const { gen, catalog } = setup(t)
  const read = await gen(await catalog('aws-iot-board-light'), { board: true })
  const control = read('components/app_logic/control.cpp')
  for (const hook of ['btn_on_single_click', 'btn_on_double_click', 'btn_on_long_press']) {
    assert.match(control, new RegExp(`__attribute__\\(\\(weak\\)\\) void ${hook}\\(void\\)`), `weak ${hook}`)
  }
  assert.match(control, /iot_button_register_cb\(btn_btn, BUTTON_DOUBLE_CLICK,/)
  assert.match(control, /iot_button_set_param\(btn_btn, BUTTON_SHORT_PRESS_TIME_MS, \(void \*\)\(intptr_t\)350\)/)
  assert.match(control, /btn_lp\.long_press\.press_time = 2000;/)
  assert.match(control, /btn_fr\.long_press\.press_time = 6000;[\s\S]*btn_factory_reset_cb/)
  assert.match(control, /nvs_flash_erase\(\);\s*esp_restart\(\);/)
  // still NO pin and no second device on the board's line
  assert.ok(!/app_button_get_or_create|gpio_config/.test(code(control)))
})

test('board_button as it was (single-click toggle): no timing override, no reset, no "no handler" noise', async (t) => {
  const { gen } = setup(t)
  const read = await gen(demo([
    { block: 'drivers/board_led_strip', prefix: 'LED', cfg: { device: 'led_strip', param_id: 'APP_DRIVER_PARAM_POWER' } },
    { block: 'drivers/board_button', prefix: 'BTN', cfg: { device: 'boot_button', target_param: 'APP_DRIVER_PARAM_POWER' } },
  ]), { board: true })
  const control = read('components/app_logic/control.cpp')
  assert.ok(!control.includes('BUTTON_SHORT_PRESS_TIME_MS'), 'the board\'s own click timing is kept')
  assert.ok(!control.includes('factory_reset'), 'no hold-to-reset unless asked for')
  assert.match(control, /#define BTN_SINGLE_CLICK_TOGGLES 1/)
  assert.match(control, /app_driver_set_param\(APP_DRIVER_PARAM_POWER, val, APP_DRIVER_SOURCE_LOCAL\);\s*btn_on_single_click\(\);/)
})

test('board_button with no target param leaves every gesture to the product', async (t) => {
  const { gen } = setup(t)
  const read = await gen(demo([
    { block: 'drivers/board_button', prefix: 'KEY', cfg: { device: 'boot_button' } },
  ]), { board: true })
  const control = read('components/app_logic/control.cpp')
  assert.ok(!control.includes('#define KEY_SINGLE_CLICK_TOGGLES'))
  assert.ok(!/app_driver_set_param/.test(control.slice(control.indexOf('key_board_button_cb'))), 'a click toggles nothing')
  assert.match(control, /static void key_board_button_cb\(void \*arg, void \*usr_data\)\n\{\n    \(void\)arg; \(void\)usr_data;\n    key_on_single_click\(\);/)
})

/* ── the board's own LED: colour ─────────────────────────────────────── */

test('board_led_strip renders HSV from the bus when hue is bound', async (t) => {
  const { gen, catalog } = setup(t)
  const read = await gen(await catalog('aws-iot-board-light'), { board: true })
  const drv = read('components/app_driver/app_driver.cpp')
  assert.match(drv, /#define LED_COLOUR_MODE 1/)
  for (const p of ['HUE', 'SATURATION', 'BRIGHTNESS']) {
    assert.match(drv, new RegExp(`if \\(param_id == APP_DRIVER_PARAM_${p}\\) \\{`), `observes ${p}`)
  }
  // an observer still: it adds no apply case for params it does not own
  assert.ok(!/case APP_DRIVER_PARAM_HUE:/.test(drv))
  assert.match(read('components/app_driver/include/app_driver_types.h'), /APP_DRIVER_PARAM_HUE,/)
})

test('board_led_strip without hue stays the on/off fixed-colour indicator', async (t) => {
  const { gen } = setup(t)
  const read = await gen(demo([
    { block: 'drivers/board_led_strip', prefix: 'LED', cfg: { device: 'led_strip', param_id: 'APP_DRIVER_PARAM_POWER' } },
  ]), { board: true })
  const drv = read('components/app_driver/app_driver.cpp')
  assert.ok(!drv.includes('LED_COLOUR_MODE 1'))
  assert.ok(!drv.includes('APP_DRIVER_PARAM_HUE'))
  assert.match(drv, /r = \(uint8_t\)\(\(uint16_t\)16\s+\* s_led_bri \/ 254U\);/)
})

/* ── review fixes ────────────────────────────────────────────────────── */

test('aws_iot_heartbeat: presence is opt-in — without it, no retained will and no retained publish', async (t) => {
  // A retained publish needs iot:RetainPublish, and AWS IoT disconnects a client
  // for a publish its policy refuses. On a heartbeat-only policy, presence ON
  // dropped every session right after connect.
  const { gen } = setup(t)
  const read = await gen(demo([
    { block: 'behaviors/aws_iot_heartbeat', prefix: 'HB', cfg: { topic_prefix: 'zerocode', period_s: 30 } },
  ], ['aws_iot']))
  const logic = code(read('components/app_logic/control.cpp'))
  assert.ok(!logic.includes('app_aws_iot_set_will'), 'no last will by default')
  assert.ok(!logic.includes('app_aws_iot_publish_retained'), 'no retained "online" by default')
  assert.match(logic, /app_aws_iot_publish\(/, 'the heartbeat itself is unchanged')
})

test('aws_iot: the connect runs on a snapshot of the config, a config change ends the backoff, and a session that does not hold is backed off', async (t) => {
  const { gen, catalog } = setup(t)
  const read = await gen(await catalog('aws-iot-conn-demo'))
  const agent = code(read('components/app_aws_iot/zc_aws_agent.cpp'))
  // the lock is held only to copy the config, never across the TLS handshake
  assert.match(agent, /if \(!snapshot_config\(&cfg\)\) \{[\s\S]*?\}\s*bool connected = session_connect\(clean_session, &cfg\);/)
  assert.ok(!/zc_aws_cfg_lock\(\);\s*bool connected = session_connect/.test(agent), 'no lock around the connect')
  assert.match(agent, /s_net\.pcClientKey = c->key;/, 'the handshake reads the copy')
  assert.match(agent, /if \(c->key != NULL\) mbedtls_platform_zeroize\(c->key, c->key_len\);/, 'the copied key is wiped')
  assert.match(agent, /if \(xTaskNotifyWait\(0, UINT32_MAX, NULL, pdMS_TO_TICKS\(delay_ms\)\) == pdTRUE\) \{\s*BackoffAlgorithm_InitializeParams/)
  assert.match(agent, /void zc_aws_config_changed\(void\)\s*\{\s*if \(s_task != NULL\) xTaskNotify\(s_task, 1, eSetBits\);/)
  assert.match(agent, /if \(\(xTaskGetTickCount\(\) - started\) >= pdMS_TO_TICKS\(ZC_AWS_STABLE_SESSION_MS\)\) \{\s*BackoffAlgorithm_InitializeParams/)
  const config = code(read('components/app_aws_iot/zc_aws_config.cpp'))
  assert.match(config, /zc_aws_cfg_lock\(\);\s*zc_aws_load_config\(\);\s*zc_aws_cfg_unlock\(\);\s*zc_aws_config_changed\(\);/)
  assert.equal((config.match(/zc_aws_config_changed\(\);/g) ?? []).length, 6, 'every setter that changes the config wakes the session')
  assert.match(config, /mbedtls_platform_zeroize\(g_zc_aws_key, g_zc_aws_key_len\);\s*free\(g_zc_aws_key\);/)
  assert.match(read('components/app_aws_iot/zc_aws_console.cpp'), /mbedtls_platform_zeroize\(text, len\);\s*free\(text\);/)
  // the demo stays on a heartbeat-only policy: presence is opt-in
  assert.ok(!code(read('components/app_logic/control.cpp')).includes('app_aws_iot_set_will'))
})

test('app_console_read_text ends a line at a lone CR, and swallows the LF of a CRLF', async (t) => {
  const { gen, catalog } = setup(t)
  const read = await gen(await catalog('aws-iot-conn-demo'))
  const src = code(read('components/app_console/app_console.cpp'))
  assert.match(src, /if \(after_cr\) \{\s*after_cr = false;\s*if \(c == '\\n'\) continue;\s*\}\s*if \(c == '\\r'\) \{ after_cr = true; c = '\\n'; \}/)
  assert.ok(!/if \(c == '\\r'\) continue;/.test(src), 'a CR is no longer dropped')
})

test('board_led_strip clamps hue, saturation and brightness to 254 (255 wrapped to off)', async (t) => {
  const { gen, catalog } = setup(t)
  const read = await gen(await catalog('aws-iot-board-light'), { board: true })
  const drv = read('components/app_driver/app_driver.cpp')
  for (const v of ['hue', 'sat', 'bri']) assert.match(drv, new RegExp(`s_led_${v} = led_scale254\\(val\\.u8\\);`), `${v} clamped`)
  assert.match(drv, /static inline uint8_t led_scale254\(uint8_t v\) \{ return v > 254 \? 254 : v; \}/)
})

test('board_button reports a gesture the button component refused, and app_logic links whole-archive', async (t) => {
  const { gen, catalog } = setup(t)
  const read = await gen(await catalog('aws-iot-board-light'), { board: true })
  const control = read('components/app_logic/control.cpp')
  assert.match(control, /if \(iot_button_register_cb\(btn_btn, BUTTON_DOUBLE_CLICK, NULL, btn_double_click_cb, NULL\) != ESP_OK\) \{/)
  assert.match(control, /\} else \{\s*ESP_LOGI\(TAG, "btn: factory reset armed/)
  // a hook override in a file of its own is linked, not silently dropped
  assert.match(read('components/app_logic/CMakeLists.txt'), /idf_component_register\([\s\S]*WHOLE_ARCHIVE[\s\S]*\)/)
})

test('mqtt-setup: Enter keeps a broker credential, "-" clears it', async (t) => {
  const { gen, catalog } = setup(t)
  const read = await gen(await catalog('mqtt-color-light'))
  const src = read('components/app_mqtt/app_mqtt.cpp')
  assert.match(src, /Broker username \(Enter keeps, - for none\)/)
  assert.match(src, /if \(strcmp\(b, "-"\) == 0\) b\[0\] = 0;\s*else if \(!b\[0\]\) strlcpy\(b, s_user, sizeof\(b\)\);/)
  assert.match(src, /if \(strcmp\(c, "-"\) == 0\) c\[0\] = 0;\s*else if \(!c\[0\]\) strlcpy\(c, s_mqtt_pass, sizeof\(c\)\);/)
})

test('board_led_strip seeds its colour from values the bus already carries', async (t) => {
  const { gen, catalog } = setup(t)
  const read = await gen(await catalog('aws-iot-board-light'), { board: true })
  const drv = read('components/app_driver/app_driver.cpp')
  for (const [p, v] of [['HUE', 'hue'], ['SATURATION', 'sat'], ['BRIGHTNESS', 'bri']]) {
    assert.match(drv, new RegExp(`if \\(app_driver_param_seen\\(APP_DRIVER_PARAM_${p}\\) && app_driver_get_param\\(APP_DRIVER_PARAM_${p}, &led_v\\) == ESP_OK\\) s_led_${v} = led_scale254\\(led_v\\.u8\\);`), `${p} seeded`)
  }
})
