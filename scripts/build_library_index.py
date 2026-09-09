#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

"""Build the browsable Library index (library.json) from the block + product
metadata. Symmetric to esp-virtual-parts' build_index.py: the canonical data
is the per-item block.yml / product.yml files; this generates a flat listing
that a UI (ZeroCode AI's public Library) fetches at runtime — so a new block or
template appears WITHOUT a UI deploy.

  python3 scripts/build_library_index.py            print the index
  python3 scripts/build_library_index.py --out F    write it to F

Output shape:
  {
    "blocks":    [ {id, name, category, description,      # id = "<category>/<name>"
                    params, driver_params, slots, idf_components} ],
    "templates": [ {id, name, description, keywords, frameworks, blocks} ],
    "models":    [ {id, description, source, symbol, size_bytes, license,
                    input, output, ops_profile, arena, task_stack_bytes,
                    chips, consumed_by, requires_psram?, blocked_by?} ]
  }
`params`/`driver_params` are the block.yml maps verbatim (type/required/
default/enum/description). `slots` is the slot names (from slots/*).
`blocks` on a template is the composed block ids in declaration order,
with `- include:` baselines resolved.
Categories come from the code_blocks/<category>/ directory (drivers,
device_types, behaviors, frameworks, peripherals, partition-tables,
sdkconfig-fragments). Names are humanized from the leaf directory.
"""
import argparse
import json
import pathlib
import sys

import yaml

ROOT = pathlib.Path(__file__).resolve().parent.parent
BLOCKS = ROOT / 'code_blocks'
PRODUCTS = ROOT / 'product_configurations'
MODELS = ROOT / 'models'


def humanize(name: str) -> str:
    return name.replace('_', ' ').replace('-', ' ').strip()


def component_deps(block_dir: pathlib.Path) -> list:
    """Managed components a block pulls, from EITHER declaration site.

    A block-root idf_component.yml is folded into block.yml's `idf_components`
    by assemble_blocks.py, so it shows up in `meta`. But a FRAMEWORK block may
    instead declare its dependency inside components/<name>/idf_component.yml —
    which is the better place (the component manager turns a component's own
    manifest into a requirement, so no PRIV_REQUIRES and no hand-written
    `espressif__` mangled name). frameworks/ml does exactly that, and the index
    used to report idf_components: [] for it, hiding esp-tflite-micro — the
    source of every model it can run.
    """
    found = set()
    for manifest in sorted(block_dir.glob('components/*/idf_component.yml')):
        try:
            data = yaml.safe_load(manifest.read_text()) or {}
        except yaml.YAMLError:
            continue
        for dep in (data.get('dependencies') or {}):
            if dep != 'idf':          # the IDF version constraint is not a component
                found.add(dep)
    return sorted(found)


def build() -> dict:
    blocks = []
    for by in sorted(BLOCKS.glob('*/*/block.yml')):
        category = by.parent.parent.name
        name = by.parent.name
        meta = yaml.safe_load(by.read_text()) or {}
        slots_dir = by.parent / 'slots'
        slot_names = sorted({f.stem for f in slots_dir.iterdir() if f.is_file()}) if slots_dir.is_dir() else []
        slot_names = sorted(set(slot_names) | set((meta.get('slots') or {}).keys()))
        blocks.append({
            'id': f'{category}/{name}',
            'name': humanize(name),
            'category': category,
            'description': meta.get('description', ''),
            'params': meta.get('params') or {},
            'driver_params': meta.get('driver_params') or {},
            'slots': slot_names,
            'idf_components': sorted(
                set((meta.get('idf_components') or {}).keys()) | set(component_deps(by.parent))
            ),
            # 'endpoint_only' = Matter endpoint with no attribute wiring; agents
            # must see this when picking a device type for a Matter product.
            **({'matter_wiring': meta['matter_wiring']} if meta.get('matter_wiring') else {}),
            **({'matter_wiring_note': meta['matter_wiring_note']} if meta.get('matter_wiring_note') else {}),
        })

    def instance_blocks(instances: list) -> list:
        """Composed block ids in declaration order, `- include:` resolved."""
        out = []
        for inst in instances or []:
            if not isinstance(inst, dict):
                continue
            if 'include' in inst:
                bl = ROOT / 'baselines' / f"{inst['include']}.yml"
                if bl.is_file():
                    sub = yaml.safe_load(bl.read_text()) or {}
                    out.extend(instance_blocks(sub.get('instances', [])))
            elif inst.get('block'):
                out.append(inst['block'])
        return out

    templates = []
    for py in sorted(PRODUCTS.glob('*/product.yml')):
        meta = yaml.safe_load(py.read_text()) or {}
        templates.append({
            'id': meta.get('id', py.parent.name),
            'name': meta.get('name', humanize(py.parent.name)),
            'description': meta.get('description', ''),
            'keywords': meta.get('keywords', []),
            'frameworks': meta.get('frameworks', []),
            'blocks': instance_blocks(meta.get('instances', [])),
        })

    # models/<id>.yml — what frameworks/ml can actually run. Emitted verbatim
    # (minus the id, which comes from the filename) so a manifest field reaches
    # the AI the day it is added, the same way block params do. Crucially this
    # carries `arena` and `blocked_by`: the agent should be able to see what a
    # model costs, and why a model it can see cannot be chosen, WITHOUT a build.
    models = []
    if MODELS.is_dir():
        for my in sorted(MODELS.glob('*.yml')):
            meta = yaml.safe_load(my.read_text()) or {}
            models.append({'id': my.stem, **meta})

    return {'blocks': blocks, 'templates': templates, 'models': models}


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', type=pathlib.Path)
    args = ap.parse_args()
    index = build()
    text = json.dumps(index, indent=2, ensure_ascii=False) + '\n'
    if args.out:
        args.out.write_text(text)
        print(f'{args.out}: {len(index["blocks"])} blocks, {len(index["templates"])} templates, '
              f'{len(index["models"])} models', file=sys.stderr)
    else:
        print(text)
