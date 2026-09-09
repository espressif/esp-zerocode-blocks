#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

"""Standalone structural check — runs on JUST this repo, no monorepo, no
engine, pyyaml only. Catches the common contributor mistakes before a PR:

    python3 scripts/check.py

What it verifies:
  - every block.yml parses; has a description; no id:/kind: (path-derived)
  - driver_params entries carry a type; params are well-formed
  - slots/ holds only .c/.h files; block dirs aren't empty
  - product.yml instances reference blocks that exist here
  - `- include: <name>` resolves to baselines/<name>.yml
  - ids/dirs line up; assembly (scripts/assemble_blocks.py) succeeds

It does NOT run the generator or a build — that's the engine's job (CI /
scripts/test-product.sh). This is the fast, offline first line of defence.
"""
import pathlib
import re
import subprocess
import sys

import yaml

ROOT = pathlib.Path(__file__).resolve().parent.parent
KINDS = {'behaviors', 'device_types', 'drivers', 'partition-tables',
         'peripherals', 'frameworks', 'sdkconfig-fragments'}
errors: list[str] = []


def err(where: str, msg: str) -> None:
    errors.append(f'{where}: {msg}')


block_ids: set[str] = set()
block_targets: dict[str, str] = {}   # block id -> chip it is scoped to (target:)
for bj in sorted(ROOT.glob('code_blocks/*/*/block.yml')):
    rel = bj.parent.relative_to(ROOT / 'code_blocks')
    bid = rel.as_posix()
    block_ids.add(bid)
    try:
        d = yaml.safe_load(bj.read_text()) or {}
    except yaml.YAMLError as e:
        err(bid, f'block.yml does not parse: {e}')
        continue
    if rel.parts[0] not in KINDS:
        err(bid, f'unknown kind folder "{rel.parts[0]}"')
    if 'id' in d or 'kind' in d:
        err(bid, 'id:/kind: must NOT be set (derived from the path)')
    if not d.get('description'):
        err(bid, 'missing description')
    # `target: <chip>` scopes a block to one chip — it applies only when
    # generating for that chip (any kind may use it). Its value must name a real
    # chip (checked against the BUILD_CHIP matrix below).
    tgt = d.get('target')
    if tgt is not None:
        if not (isinstance(tgt, str) and tgt):
            err(bid, 'target: must be a chip name (e.g. esp32h2)')
        else:
            block_targets[bid] = tgt
    for name, spec in (d.get('driver_params') or {}).items():
        if not isinstance(spec, dict) or 'type' not in spec:
            err(bid, f'driver_param "{name}" needs a type')
    slots = bj.parent / 'slots'
    slot_names: set[str] = set()
    slot_text: dict[str, str] = {}
    if slots.is_dir():
        for f in slots.iterdir():
            if not f.is_file():
                continue
            if f.suffix not in ('.c', '.h'):
                err(bid, f'slots/{f.name}: only .c/.h allowed')
            else:
                slot_names.add(f.stem)
                slot_text[f.stem] = f.read_text()
                # THE LEGACY I2C DRIVER IS GONE, and it must not come back.
                # driver/i2c.h is end-of-life in IDF 6 and REMOVED in 7, but
                # that is the smaller reason. The bigger one: a bus created by
                # i2c_new_master_bus() — which is every bus in this catalog now,
                # and every bus esp_board_manager creates — cannot be opened by
                # a legacy consumer, so one block written the old way builds,
                # links, boots and is silent on the device. There is no mixed
                # state, which is why this is a hard error and not a warning.
                # See CLAUDE.md → How an I2C block gets its bus.
                # An INCLUDE, not a mention: several blocks name the old
                # header in a comment explaining what they no longer do.
                if re.search(r'^\s*#\s*include\s*[<"]driver/i2c\.h[>"]', slot_text[f.stem], re.M):
                    err(bid, f'slots/{f.name} includes the legacy <driver/i2c.h>. '
                             f'Use <driver/i2c_master.h>: resolve the port with '
                             f'i2c_master_get_bus_handle(), attach a device with '
                             f'i2c_master_bus_add_device(), and transfer with '
                             f'i2c_master_transmit/receive/transmit_receive.')

    # ── FreeRTOS IS NOT TRANSITIVE. ────────────────────────────────────────
    # <driver/i2c.h> used to pull FreeRTOS in (its timeouts are TickType_t) and
    # <driver/i2c_master.h> does not, so a slot calling vTaskDelay() with no
    # include of its own compiled ONLY in products where some other block in
    # the same file happened to supply it. That is a latent break: the block is
    # fine in every product it has been tried in and fails the day it is put in
    # a product on its own — and the error is a compile error in generated
    # code, which is the least legible place we have.
    #
    # Checked PER SLOT FAMILY, not per block: driver_* slots land in
    # app_driver.cpp, logic_* in app_logic/<concern>.cpp, matter_* in
    # app_matter.cpp — separate translation units, so an include in one family
    # does nothing for another. The family's own <family>_includes slot is
    # where the include belongs.
    RTOS_SYMS = re.compile(
        r'\b(vTaskDelay|vTaskDelete|vTaskSuspend|vTaskResume|xTaskCreate'
        r'|xTaskCreatePinnedToCore|xTaskGetTickCount|xTaskNotify\w*|TaskHandle_t'
        r'|pdMS_TO_TICKS|portTICK_PERIOD_MS|taskYIELD)\b')
    RTOS_INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]freertos/task\.h[>"]', re.M)
    families: dict[str, list[str]] = {}
    for name in sorted(slot_names):
        families.setdefault(name.split('_', 1)[0], []).append(name)
    for fam, names in sorted(families.items()):
        users = [n for n in names if RTOS_SYMS.search(slot_text[n])]
        if not users:
            continue
        if any(RTOS_INCLUDE.search(slot_text[n]) for n in names):
            continue
        err(bid, f'slots/{users[0]} uses the FreeRTOS task API but no {fam}_* slot '
                 f'includes <freertos/task.h>. It is not pulled in transitively; '
                 f'add the include to slots/{fam}_includes.h (each slot family is '
                 f'its own translation unit).')

    # ── A BLOCK THAT TALKS ON A SHARED BUS MUST SAY SO. ────────────────────
    # Same failure shape as the legacy-header rule above and the same reason it
    # is an error: an undeclared consumer composed above (or without) its bus
    # provider generates, compiles, links and boots, and is silent on the
    # device. The declaration is what lets the validator order the instances.
    # Found this way: drivers/speaker_es8311 configured an ES8311 over I2C from
    # driver_init and sat above peripherals/i2c_bus in mosaico-touch-hub.
    BUS_CALLS = re.compile(
        r'\b(i2c_master_get_bus_handle|i2c_master_bus_add_device|i2c_master_transmit'
        r'|i2c_master_receive|i2c_master_transmit_receive|i2c_new_master_bus'
        r'|audio_codec_new_i2c_ctrl)\b')
    # Blocks that CREATE and own their bus inside a display node-init slot.
    # Those slots run in display_setup_task, which is not ordered against
    # driver_init, so blessing them as providers would bless a race — they are
    # deliberately outside the bus contract in both directions.
    # See CLAUDE.md → Shared buses ("Two things this deliberately does NOT do").
    BUS_EXEMPT = {'drivers/display_touch_cst9220', 'drivers/display_touch_ft5x06'}
    bus_users = sorted(n for n in slot_names if BUS_CALLS.search(slot_text[n]))
    if bus_users and bid not in BUS_EXEMPT and not (d.get('requires_bus') or d.get('provides_bus')):
        err(bid, f'slots/{bus_users[0]} performs bus transactions but the block declares '
                 f'neither requires_bus: nor provides_bus:. Undeclared, "some other '
                 f'instance configured this port" is convention only — the product '
                 f'builds and the device is silent. See CLAUDE.md → Shared buses.')

    # ── A MATTER ENDPOINT WITH NO WIRING MUST SAY SO. ──────────────────────
    # 24 of the 53 device_types create a Matter endpoint and wire no
    # attributes: the device appears on the fabric but reflects and accepts
    # nothing. That is a legitimate state (stubs, composed parents, input-only
    # controllers) — but agents and the validator must be able to see it, so
    # it is a FIELD (matter_wiring: endpoint_only), kept honest here: required
    # exactly when no matter_*_cb_cases slot exists, forbidden when one does.
    if rel.parts[0] == 'device_types' and 'matter_endpoint_create' in slot_names:
        wired = {'matter_driver_cb_cases', 'matter_attr_cb_cases'} & slot_names
        binding = d.get('matter_wiring')
        if not wired and binding != 'endpoint_only':
            err(bid, 'creates a Matter endpoint but wires no attributes — declare '
                     'matter_wiring: endpoint_only so the validator and agents can see it')
        if wired and binding == 'endpoint_only':
            err(bid, 'matter_wiring: endpoint_only, but matter_*_cb_cases slots exist — drop the field')

    # bmgr: — the esp-board-manager mapping (see engine/src/types.ts).
    # Checked here because every mistake below is invisible on a boardless
    # product and only shows up as wrong HARDWARE on a board one.
    bmgr = d.get('bmgr')
    if bmgr is not None:
        params = set(d.get('params') or {}) | set(d.get('driver_params') or {})
        if not isinstance(bmgr, dict):
            err(bid, 'bmgr: must be a mapping')
        else:
            if bmgr.get('init') != 'board_manager':
                err(bid, f'bmgr.init must be "board_manager" (got {bmgr.get("init")!r})')
            for s in bmgr.get('replaces_slots') or []:
                # A typo here leaves the block's own init in place AND hands the
                # line to bmgr — two owners, which is the thing this prevents.
                if s not in slot_names:
                    err(bid, f'bmgr.replaces_slots names "{s}", which is not a slots/ file')
            pb = bmgr.get('provided_by')
            if pb is not None:
                # A board DEVICE TYPE (board_devices.yaml `type:`). A typo here
                # matches no board, so the block never steps aside and its init
                # races the board's for the same hardware — silent until a boot
                # on the one board it was written for.
                if not isinstance(pb, str) or not pb:
                    err(bid, 'bmgr.provided_by must be a non-empty board device type')
                elif not bmgr.get('replaces_slots') and not bmgr.get('device_param'):
                    # provided_by does one of two jobs: gate replaces_slots (the
                    # block steps aside), or say which board device TYPE an
                    # adapter binds to. With neither it reads like a
                    # declaration and does nothing.
                    err(bid, 'bmgr.provided_by has neither replaces_slots to gate '
                             'nor device_param to type — it does nothing')
            dp = bmgr.get('device_param')
            if dp is not None:
                # The cfg key holding the board DEVICE NAME. Matched by type,
                # fetched by name — a typo in either resolves to no device, and
                # without these two checks the block still generates and builds.
                if not isinstance(dp, str) or dp not in (d.get('params') or {}):
                    err(bid, f'bmgr.device_param must name a params key holding the '
                             f'board device name (got {dp!r})')
                if not pb:
                    err(bid, 'bmgr.device_param needs bmgr.provided_by — a board device '
                             'is matched by TYPE and only then fetched by name')
            for g in bmgr.get('requires') or []:
                if not isinstance(g, dict) or {'cfg', 'equals', 'reason'} - set(g):
                    err(bid, f'bmgr.requires entry needs cfg/equals/reason: {g!r}')
                elif g['cfg'] not in params:
                    # A guard on an undeclared param can never fail.
                    err(bid, f'bmgr.requires guards cfg.{g["cfg"]}, which the block does not declare')
            for kind in ('peripherals', 'devices'):
                for e in bmgr.get(kind) or []:
                    if not isinstance(e, dict) or not e.get('name'):
                        err(bid, f'bmgr.{kind} entry needs a name (bmgr merges fragments by name)')
                        continue
                    sk = e.get('shared_key')
                    if sk is None:
                        continue
                    # A shared fragment is emitted ONCE per distinct resolved
                    # key. A key that does not vary with the resource's own
                    # identity collapses two different buses into one.
                    if not isinstance(sk, (str, int)):
                        err(bid, f'bmgr.{kind} "{e["name"]}": shared_key must be a string or number')
                    elif isinstance(sk, str) and '{{' not in sk:
                        err(bid, f'bmgr.{kind} "{e["name"]}": shared_key "{sk}" has no {{{{…}}}} — '
                                 'a constant key makes every instance the same resource')

    # provides_bus / requires_bus — a block CONFIGURES a shared bus, or only
    # TALKS on one. Undeclared, "some other instance configured this port" is
    # an assumption held by convention: the product builds and the device is
    # silent. See engine/src/types.ts (BusDecl).
    for key in ('provides_bus', 'requires_bus'):
        decl = d.get(key)
        if decl is None:
            continue
        entries = decl if isinstance(decl, list) else [decl]
        for e in entries:
            if not isinstance(e, dict):
                err(bid, f'{key} entry must be a mapping with type: and port_param:')
                continue
            if not e.get('type'):
                err(bid, f'{key} entry needs a type (i2c, spi, …)')
            pp = e.get('port_param')
            if not pp:
                err(bid, f'{key} entry needs a port_param naming the cfg key holding the bus index')
            elif pp not in (d.get('params') or {}):
                # Resolves to an empty port and matches the wrong thing.
                err(bid, f'{key} names port_param "{pp}", which is not in params')

INSTANCE_KEYS = {'block', 'prefix', 'cfg', 'include'}
baselines = {p.stem for p in ROOT.glob('baselines/*.yml')}
for pj in sorted(ROOT.glob('product_configurations/*/product.yml')):
    pid = pj.parent.name
    try:
        d = yaml.safe_load(pj.read_text()) or {}
    except yaml.YAMLError as e:
        err(pid, f'product.yml does not parse: {e}'); continue
    for i, inst in enumerate(d.get('instances') or []):
        if 'include' in inst:
            if inst['include'] not in baselines:
                err(pid, f'instance {i}: include "{inst["include"]}" has no baselines/*.yml')
        elif 'block' in inst:
            if inst['block'] not in block_ids:
                err(pid, f'instance {i}: unknown block "{inst["block"]}"')
        else:
            err(pid, f'instance {i}: neither block: nor include:')
        # A misspelt key does not fail loudly on its own: the engine reads
        # `inst.cfg ?? {}`, so anything else silently becomes an empty config
        # and the instance is generated with defaults. Where a block has no
        # required-without-default param that produces no error at all, and a
        # driver_param quietly falls back to its default bus name.
        for key in inst:
            if key not in INSTANCE_KEYS:
                err(pid, f'instance {i}: unknown key "{key}" '
                         f'(expected one of {", ".join(sorted(INSTANCE_KEYS))})')

    # A PRODUCT.YML NEVER NAMES A BOARD. The catalog describes what a product
    # DOES; the board describes what hardware one instance of it RUNS ON, and
    # that is a property of a user's product, not of a catalog entry. `board:`
    # here would weld all 76 general products to somebody's desk again, so it
    # is refused in code rather than by convention.
    if 'board' in d:
        err(pid, 'product.yml must not carry `board:` — the board is a separate '
                 'board.yaml (schema zc-board/1); a catalog product is general')

# board.yaml — the hardware one instance runs on. Only the two board SAMPLE
# products carry one here; the shape is checked so a typo degrades loudly
# instead of silently generating a boardless tree that still reports
# "build succeeded".
BOARD_RESOLUTIONS = {'exact', 'fallback', 'custom'}
for bf in sorted(ROOT.glob('product_configurations/*/board.yaml')):
    pid = bf.parent.name
    try:
        b = yaml.safe_load(bf.read_text()) or {}
    except yaml.YAMLError as e:
        err(pid, f'board.yaml does not parse: {e}'); continue
    if b.get('schema') not in (None, 'zc-board/1'):
        err(pid, f'board.yaml: schema must be "zc-board/1" (got {b.get("schema")!r})')
    # `selected` is ALWAYS present: the user picks hardware even when it is a
    # bare module.
    if not isinstance(b.get('selected'), str) or not b['selected']:
        err(pid, 'board.yaml: selected: is required (an esp-virtual-parts board id)')
    if not isinstance(b.get('chip'), str) or not b['chip']:
        err(pid, 'board.yaml: chip: is required')
    bmgr = b.get('bmgr')
    if bmgr is not None:
        if not isinstance(bmgr, dict):
            err(pid, 'board.yaml: bmgr: must be a mapping')
        else:
            # null is legal: no bmgr definition for this hardware YET. That is
            # a coverage fact about our packs, not an error about the board.
            if not (bmgr.get('board') is None or isinstance(bmgr.get('board'), str)):
                err(pid, 'board.yaml: bmgr.board must be a board name or null')
            if bmgr.get('resolved_from') not in BOARD_RESOLUTIONS:
                err(pid, f'board.yaml: bmgr.resolved_from must be one of '
                         f'{"|".join(sorted(BOARD_RESOLUTIONS))} (got {bmgr.get("resolved_from")!r})')

# TARGET-SCOPED BLOCKS: a block's `target:` must name a real chip — a typo
# matches no build and silently drops the block everywhere. chips.txt is the
# canonical list (hardware.ts CHIP_MODULES is the wider "describable" set).
build_chips = {l.strip() for l in (ROOT / 'chips.txt').read_text().splitlines()
               if l.strip() and not l.startswith('#')}
if not build_chips:
    err('chips', 'chips.txt is empty')
for bid, tgt in sorted(block_targets.items()):
    if tgt not in build_chips:
        err(bid, f'target "{tgt}" is not a supported chip (chips.txt)')
# FRAMEWORK CHIPS: every framework says which chips it builds for, and only
# with names from chips.txt — CI reads this to decide what to compile.
for bd in sorted((ROOT / 'code_blocks' / 'frameworks').iterdir()):
    by = bd / 'block.yml'
    if not by.is_file():
        continue
    fw = yaml.safe_load(by.read_text()) or {}
    fid = f'frameworks/{bd.name}'
    if not isinstance(fw.get('chips'), list) or not fw['chips']:
        err(fid, 'framework block must declare `chips: [...]` (the chips it builds for)')
        continue
    for ch in fw['chips']:
        if ch not in build_chips:
            err(fid, f'chips: "{ch}" is not in chips.txt')
    # RADIO: every framework says whether it needs one — the generator drops
    # the ESP32-P4's hosted stack from an all-local tree on the strength of it.
    if not isinstance(fw.get('radio'), bool):
        err(fid, 'framework block must declare `radio: true|false` (needs a radio?)')

# assembly must succeed (proves the authoring→consumption transform is valid)
r = subprocess.run([sys.executable, str(ROOT / 'scripts/assemble_blocks.py'), '--check'],
                   capture_output=True, text=True)
if r.returncode != 0:
    err('assemble', r.stderr.strip() or 'assembly failed')

if errors:
    print('\n'.join(f'ERROR {e}' for e in errors))
    print(f'\n{len(errors)} error(s)')
    sys.exit(1)
print(f'OK: {len(block_ids)} blocks, {len(baselines)} baselines, '
      f'{len(list(ROOT.glob("product_configurations/*/product.yml")))} products — structurally valid')
