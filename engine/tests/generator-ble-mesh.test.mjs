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
