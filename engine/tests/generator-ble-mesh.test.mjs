#!/usr/bin/env node --test
// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

/**
 * The BLE Mesh framework's guarantees, each one a way a hand-written mesh node
 * silently did not work:
 *
 *   - a command leaves through the CLIENT model (esp_ble_mesh_generic_client_
 *     set_state) with a full message context, not as a raw publish on a
 *     context nobody set up;
 *   - it carries a FRESH transaction id, without which a receiver discards
 *     every message after the first as a retransmission;
 *   - "am I provisioned" is the stack's own answer, restored from settings,
 *     rather than a flag some callback happened to set — which is what leaves
 *     a light blinking "joining" forever after a reboot it already survived;
 *   - the sdkconfig carries the keys the code needs to LINK and to REMEMBER
 *     (CFG_CLI, the generic clients, SETTINGS);
 *   - the mesh library is in the component's REQUIRES;
 *   - and a provisioner binds the application key to a node's CLIENTS as well
 *     as its servers, which is the difference between a remote that works and
 *     one that joins the network and then never says anything.
 *
 * Generates over the REAL assembled catalog (like scripts/validate.mjs), so a
 * framework or slot edit that undoes any of them fails here rather than on a
 * bench weeks later.
 */
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { execFileSync } from 'node:child_process'
import { mkdtempSync, readFileSync, rmSync, existsSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const eng = await import(join(ROOT, 'engine/dist/index.js'))

function assembled() {
  const out = join(mkdtempSync(join(tmpdir(), 'zc-gen-')), 'templates')
  execFileSync('python3', [join(ROOT, 'scripts/assemble_blocks.py'),
    '--out', join(out, 'code_blocks'), '--products-out', join(out, 'product_configurations')], { stdio: 'ignore' })
  return out
}

async function generateProduct(templatesDir, id) {
  const paths = { templatesDir, baseFirmwareDir: join(ROOT, 'base_firmware') }
  const products = await eng.listAllProducts(paths)
  const entry = products.find((p) => p.product.id === id || p.id === id)
  assert.ok(entry, `product ${id} in the catalog`)
  const outDir = mkdtempSync(join(tmpdir(), `zc-out-${id}-`))
  await eng.generate(paths, { product: entry.product, board: null, outDir, chip: 'esp32c6' })
  return outDir
}

function readAll(dir, rel) {
  const p = join(dir, rel)
  assert.ok(existsSync(p), `${rel} generated`)
  return readFileSync(p, 'utf-8')
}

test('a mesh node sends through its client model, with a TID, and asks the stack whether it is provisioned', async (t) => {
  const tpl = assembled()
  t.after(() => rmSync(dirname(tpl), { recursive: true, force: true }))
  const out = await generateProduct(tpl, 'ble-mesh-light')
  t.after(() => rmSync(out, { recursive: true, force: true }))

  const app = readAll(out, 'components/app_ble_mesh/app_ble_mesh.cpp')

  // Commands go out through the client model with a real context, never as a
  // bare publish on a pub context nobody filled in.
  assert.match(app, /esp_ble_mesh_generic_client_set_state\(&common, &set\)/,
    'publishes through the Generic client model')
  assert.match(app, /common->ctx\.app_idx = app_idx/, 'the message context carries an app key index')
  assert.match(app, /common->ctx\.send_ttl = ZC_MESH_SEND_TTL/, 'the message context carries a TTL')
  assert.match(app, /set\.onoff_set\.tid = s_tid\+\+/, 'OnOff Set carries an incrementing TID')
  assert.match(app, /set\.level_set\.tid = s_tid\+\+/, 'Level Set carries an incrementing TID')

  // A command to the default group follows the client model's configured
  // publication address — the room group a provisioner chose — and both
  // publish paths resolve it before building the context (without it, a group
  // picked in the app was ignored and every command went to 0xC000).
  assert.match(app, /static uint16_t client_dest\(esp_ble_mesh_client_t \*client, uint16_t addr\)/)
  assert.match(app, /addr == ZC_MESH_GROUP_ADDR && client && client->model && client->model->pub &&\s+client->model->pub->publish_addr != ESP_BLE_MESH_ADDR_UNASSIGNED/)
  assert.match(app, /return client->model->pub->publish_addr;/)
  for (const c of ['s_onoff_client', 's_level_client']) {
    assert.match(app, new RegExp(`addr = client_dest\\(&${c}, addr\\);\\s+esp_ble_mesh_client_common_param_t common = \\{\\};\\s+esp_err_t err = client_msg_common\\(&common, &${c},`), `${c} resolves its destination first`)
  }

  // Every model is defined WITH a publication context — a NULL pub is the
  // crash this rules out by construction.
  for (const pub of ['zc_onoff_srv_pub', 'zc_level_srv_pub', 'zc_onoff_cli_pub', 'zc_level_cli_pub']) {
    assert.match(app, new RegExp(`ESP_BLE_MESH_MODEL_PUB_DEFINE\\(${pub},`), `${pub} defined`)
    assert.match(app, new RegExp(`&${pub},`), `${pub} handed to its model`)
  }
  assert.doesNotMatch(app, /ESP_BLE_MESH_MODEL_GEN_ONOFF_CLI\(NULL/, 'the OnOff client is never given a NULL pub')

  // Provisioned-ness is the stack's answer, restored from settings.
  assert.match(app, /return esp_ble_mesh_node_is_provisioned\(\)/,
    'zc_ble_mesh_provisioned reads the stack, not a local flag')

  // The unprovisioned beacon carries the marker a provisioner matches on.
  assert.match(app, /s_dev_uuid\[16\] = \{ ZC_MESH_UUID_B0, ZC_MESH_UUID_B1 \}/,
    'the device UUID starts with the fixed marker')
  assert.match(app, /memcpy\(s_dev_uuid \+ 2, s_bd_addr/, 'the rest of the UUID is this device address')

  // Both bearers, a group address, and a way back out of the network.
  assert.match(app, /ESP_BLE_MESH_PROV_ADV \| ESP_BLE_MESH_PROV_GATT/, 'PB-ADV and PB-GATT')
  assert.match(app, /esp_ble_mesh_node_local_reset\(\)/, 'factory reset resets the node and erases settings')
  assert.match(app, /case ESP_BLE_MESH_NODE_PROV_RESET_EVT:[\s\S]{0,400}esp_ble_mesh_node_prov_enable/,
    'after a reset the node advertises again without a reboot')
  assert.match(app, /"mesh"/, 'the mesh console command is registered')

  // The device type is bound in BOTH directions.
  assert.match(app, /zc_ble_mesh_set_onoff_state\(val\.b\)/, 'bus change reaches the OnOff server')
  assert.match(app, /apply_onoff_from_mesh/, 'mesh OnOff writes reach the bus')
  assert.match(app, /apply_level_from_mesh/, 'mesh Level writes reach the bus')

  // The build inputs the code needs.
  const cmake = readAll(out, 'components/app_ble_mesh/CMakeLists.txt')
  assert.match(cmake, /PRIV_REQUIRES[^\n]*\bbt\b/, 'the mesh/BLE library is in PRIV_REQUIRES')

  const sdk = readAll(out, 'sdkconfig.defaults')
  for (const key of ['CONFIG_BLE_MESH=y', 'CONFIG_BLE_MESH_NODE=y', 'CONFIG_BLE_MESH_CFG_CLI=y',
    'CONFIG_BLE_MESH_SETTINGS=y', 'CONFIG_BLE_MESH_GENERIC_SERVER=y',
    'CONFIG_BLE_MESH_GENERIC_ONOFF_CLI=y', 'CONFIG_BLE_MESH_GENERIC_LEVEL_CLI=y',
    'CONFIG_BLE_MESH_PB_ADV=y', 'CONFIG_BLE_MESH_PB_GATT=y']) {
    assert.ok(sdk.includes(key), `${key} in sdkconfig.defaults`)
  }
})

test('the provisioner configures a node it added, clients included', async (t) => {
  const tpl = assembled()
  t.after(() => rmSync(dirname(tpl), { recursive: true, force: true }))
  const out = await generateProduct(tpl, 'ble-mesh-switch')
  t.after(() => rmSync(out, { recursive: true, force: true }))

  const app = readAll(out, 'components/app_ble_mesh/app_ble_mesh.cpp')

  assert.match(app, /esp_ble_mesh_provisioner_set_dev_uuid_match\(/,
    'matches unprovisioned beacons on the marker')
  assert.match(app, /esp_ble_mesh_provisioner_add_unprov_dev\(/, 'adds a matching device')
  assert.match(app, /esp_ble_mesh_provisioner_prov_enable\(/, 'provisioning is enabled')
  assert.match(app, /esp_ble_mesh_provisioner_add_local_app_key\(/, 'the network has an application key')
  assert.match(app, /esp_ble_mesh_config_client_set_state\(&common, &set\)/,
    'the Config Client drives the post-provisioning configuration')

  // The step table is the guarantee: key added, bound to BOTH servers AND
  // BOTH clients, servers subscribed to the group, clients publishing to it.
  const steps = app.slice(app.indexOf('s_prov_steps[] ='), app.indexOf('zc_prov_parse_app_key'))
  for (const line of [
    'ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD',
    'ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND, ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_SRV',
    'ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND, ESP_BLE_MESH_MODEL_ID_GEN_LEVEL_SRV',
    'ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND, ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_CLI',
    'ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND, ESP_BLE_MESH_MODEL_ID_GEN_LEVEL_CLI',
    'ESP_BLE_MESH_MODEL_OP_MODEL_SUB_ADD,  ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_SRV',
    'ESP_BLE_MESH_MODEL_OP_MODEL_PUB_SET,  ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_CLI',
  ]) {
    assert.ok(steps.includes(line), `configuration step present: ${line}`)
  }
  assert.match(app, /set\.model_sub_add\.sub_addr = ZC_PROV_GROUP_ADDR/, 'servers subscribe to the group')
  assert.match(app, /set\.model_pub_set\.publish_addr = ZC_PROV_GROUP_ADDR/, 'clients publish to the group')
  assert.match(app, /esp_ble_mesh_provisioner_bind_app_key_to_local_model\(/,
    'the provisioner binds the key to its OWN client models too')

  // A provisioner must not beacon for a provisioner of its own.
  assert.match(app, /s_advertise_unprovisioned = false/, 'node advertising is switched off')

  // The controller device types SEND rather than hold state.
  assert.match(app, /zc_ble_mesh_publish_onoff\(ZC_MESH_GROUP_ADDR/, 'the remote publishes OnOff to the group')
  assert.match(app, /zc_ble_mesh_publish_level\(ZC_MESH_GROUP_ADDR/, 'the remote publishes Level to the group')
})

test('a switch toggles and dims relative to the lights, and products get hooks instead of callbacks', async (t) => {
  const tpl = assembled()
  t.after(() => rmSync(dirname(tpl), { recursive: true, force: true }))
  const out = await generateProduct(tpl, 'ble-mesh-switch')
  t.after(() => rmSync(out, { recursive: true, force: true }))

  const app = readAll(out, 'components/app_ble_mesh/app_ble_mesh.cpp')

  // The hooks: weak, C linkage (the header declares them inside extern "C";
  // a default with the other linkage is a different symbol and an override
  // would never bind), and each one actually called by the framework.
  for (const [hook, sig] of [
    ['zc_ble_mesh_on_onoff_status', 'uint16_t src, bool on'],
    ['zc_ble_mesh_on_level_status', 'uint16_t src, int16_t level'],
    ['zc_ble_mesh_on_provisioned', 'bool provisioned'],
  ]) {
    assert.ok(app.includes(`extern "C" __attribute__((weak)) void ${hook}(${sig})`),
      `${hook} has a weak extern "C" default`)
    const calls = app.split(`${hook}(`).length - 2   // minus the definition
    assert.ok(calls >= 1, `${hook} is called by the framework`)
  }
  assert.match(app, /case ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT:[\s\S]{0,400}zc_ble_mesh_on_provisioned\(true\)/,
    'provisioning complete reaches the hook')
  assert.match(app, /case ESP_BLE_MESH_NODE_PROV_RESET_EVT:[\s\S]{0,600}zc_ble_mesh_on_provisioned\(false\)/,
    'a node reset reaches the hook')
  assert.match(app, /already provisioned, address[\s\S]{0,200}zc_ble_mesh_on_provisioned\(true\)/,
    'a node that boots provisioned reports it once')

  // Toggle = inverse of the last state heard or sent; a Status updates it.
  assert.match(app, /esp_err_t zc_ble_mesh_publish_toggle\(uint16_t addr\)\s*\{[\s\S]{0,300}zc_ble_mesh_publish_onoff\(addr, !s_last_onoff\)/,
    'toggle inverts the remembered state')
  assert.match(app, /static volatile bool s_last_onoff = false;/, 'first toggle after boot turns the lights on')
  assert.match(app, /ESP_BLE_MESH_GENERIC_CLIENT_PUBLISH_EVT[\s\S]{0,1200}s_last_onoff = on;[\s\S]{0,200}zc_ble_mesh_on_onoff_status\(src, on\)/,
    'a received OnOff Status updates the toggle state and fires the hook')
  // ...but not one heard right after a send: it may answer the previous press.
  assert.match(app, /s_last_onoff = on;\s*s_last_onoff_send_us = esp_timer_get_time\(\);/,
    'a send records its time')
  assert.match(app, /if \(esp_timer_get_time\(\) - s_last_onoff_send_us > ZC_MESH_STATUS_HOLDOFF_US\) \{\s*s_last_onoff = on;\s*\}/,
    'a Status inside the holdoff does not overwrite the toggle state')
  assert.match(app, /esp_ble_mesh_model_subscribe_group_addr\(/, 'clients subscribe to hear the lights')

  // Delta: Generic Delta Set Unack, TID reused only within a unicast gesture.
  assert.match(app, /esp_err_t zc_ble_mesh_publish_delta\(uint16_t addr, int32_t delta, bool continue_hold\)/)
  assert.match(app, /ESP_BLE_MESH_MODEL_OP_GEN_DELTA_SET_UNACK/, 'dimming is a Delta Set')
  assert.match(app, /bool same = continue_hold && unicast && s_delta_open/, 'continue_hold continues a unicast transaction')
  assert.match(app, /s_delta_tid = s_tid\+\+;/, 'a new gesture takes a fresh TID')
  assert.match(app, /set\.delta_set\.tid = s_delta_tid;/)

  // The framework is still the only place callbacks are registered.
  assert.equal(app.split('esp_ble_mesh_register_generic_client_callback(').length - 1, 1)

  // The scene button toggles through the framework, not a private flag.
  assert.match(app, /zc_ble_mesh_publish_toggle\(ZC_MESH_GROUP_ADDR\)/)
  assert.doesNotMatch(app, /_mesh_on = !/, 'no private toggle flag')

  // The provisioner points the lights' status at the group.
  const steps = app.slice(app.indexOf('s_prov_steps[] ='), app.indexOf('zc_prov_parse_app_key'))
  assert.ok(steps.includes('ESP_BLE_MESH_MODEL_OP_MODEL_PUB_SET,  ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_SRV'))

  // The contract both implementations honour.
  const h = readAll(out, 'components/app_ble_mesh/include/app_ble_mesh.h')
  const cBlock = h.slice(h.indexOf('extern "C" {'), h.lastIndexOf('#ifdef __cplusplus'))
  for (const decl of ['esp_err_t zc_ble_mesh_publish_toggle(uint16_t addr);',
    'esp_err_t zc_ble_mesh_publish_delta(uint16_t addr, int32_t delta, bool continue_hold);',
    'void zc_ble_mesh_on_onoff_status(uint16_t src, bool on);',
    'void zc_ble_mesh_on_level_status(uint16_t src, int16_t level);',
    'void zc_ble_mesh_on_provisioned(bool provisioned);']) {
    assert.ok(cBlock.includes(decl), `${decl} declared inside extern "C"`)
  }
})

test('app_logic can include app_ble_mesh.h to define the hooks, and only on a mesh product', async (t) => {
  const tpl = assembled()
  t.after(() => rmSync(dirname(tpl), { recursive: true, force: true }))
  const privRequires = (dir) => {
    const cmake = readAll(dir, 'components/app_logic/CMakeLists.txt')
    const m = cmake.match(/PRIV_REQUIRES([^)]*)\)/)
    assert.ok(m, 'app_logic has PRIV_REQUIRES')
    return m[1].split(/\s+/).filter(Boolean)
  }
  const mesh = await generateProduct(tpl, 'ble-mesh-switch')
  t.after(() => rmSync(mesh, { recursive: true, force: true }))
  assert.ok(privRequires(mesh).includes('app_ble_mesh'), 'mesh product: app_logic requires app_ble_mesh')
  // No cycle: the framework component never needs the product's logic.
  assert.ok(!readAll(mesh, 'components/app_ble_mesh/CMakeLists.txt').includes('app_logic'))

  const plain = await generateProduct(tpl, 'smart-plug')
  t.after(() => rmSync(plain, { recursive: true, force: true }))
  assert.ok(!privRequires(plain).includes('app_ble_mesh'), 'non-mesh product: no app_ble_mesh')
})
