#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

"""Assemble authoring-format blocks (block.yml + slots/*.c|h) into the
consumption format the ZeroCode generator reads (single block.yml with
inline slot scalars). Used by sync-to-zerocode.sh; the platform never
sees the split format.

  python3 scripts/assemble_blocks.py --out <dir>     assemble the tree
  python3 scripts/assemble_blocks.py --check         assemble to temp + parse every result
"""
import argparse
import pathlib
import re
import shutil
import sys
import tempfile

import yaml

ROOT = pathlib.Path(__file__).resolve().parent.parent

# authoring folder → the `kind:` the generator expects
FOLDER_KIND = {
    'behaviors': 'behavior',
    'device_types': 'device_type',
    'drivers': 'driver',
    'partition-tables': 'partition_table',
    'peripherals': 'peripheral',
    'frameworks': 'framework',
    'sdkconfig-fragments': 'sdkconfig_fragment',
}


def _section_bounds(lines: list[str], key: str):
    start = next((i for i, l in enumerate(lines) if l.split('#')[0].rstrip() == f'{key}:'), None)
    if start is None:
        return None
    end = len(lines)
    for j in range(start + 1, len(lines)):
        l = lines[j]
        if l.strip() and not l.startswith(' ') and not l.startswith('#'):
            end = j
            break
    return start, end


def _sdk_parse(path: pathlib.Path) -> dict:
    out: dict = {}
    for ln in path.read_text().splitlines():
        ln = ln.strip()
        if not ln or ln.startswith('#'):
            continue
        k, _, v = ln.partition('=')
        out[k] = v[1:-1] if v.startswith('"') else int(v)
    return out


def expand_manifest(block_dir: pathlib.Path) -> str:
    """Authoring manifest + sidecar files → consumption manifest (no slots):
    - id/kind derived from the path (authors never write them)
    - driver_params section → merged into params (`type: <t>` → `type: string`)
      + a param_refs section
    - requires.txt / sdkconfig.defaults / idf_component.yml → the
      cmake_priv_requires / sdkconfig / idf_components sections
    """
    text = (block_dir / 'block.yml').read_text()
    parsed = yaml.safe_load(text)
    rel = block_dir.relative_to(ROOT / 'code_blocks')

    header = ''
    if 'id' not in parsed:
        header += f'id: {rel.as_posix()}\n'
    if 'kind' not in parsed:
        header += f'kind: {FOLDER_KIND[rel.parts[0]]}\n'

    # driver_params → params entries (+ collected refs)
    refs = {n: spec['type'] for n, spec in (parsed.get('driver_params') or {}).items()}
    lines = text.split('\n')
    dpb = _section_bounds(lines, 'driver_params')
    if dpb:
        moved = [re.sub(r'type:\s*(\w+)', 'type: string', l, count=1) if re.search(r'\btype:', l) else l
                 for l in lines[dpb[0] + 1:dpb[1]]]
        while moved and not moved[-1].strip():
            moved.pop()
        del lines[dpb[0]:dpb[1]]
        pb = _section_bounds(lines, 'params')
        if pb:
            insert_at = pb[1]
            while insert_at > pb[0] and not lines[insert_at - 1].strip():
                insert_at -= 1
            lines[insert_at:insert_at] = moved
        else:
            lines.extend(['params:'] + moved)
    body = '\n'.join(lines)
    body = re.sub(r'\n{3,}', '\n\n', body).rstrip('\n') + '\n'

    tail = ''
    if refs:
        tail += '\nparam_refs:\n' + ''.join(f'  {n}: {t}\n' for n, t in refs.items())

    req = block_dir / 'requires.cmake'
    if req.is_file():
        tail += '\ncmake_priv_requires:\n'
        for comp, deps in re.findall(r'list\(APPEND\s+(\S+)_PRIV_REQUIRES\s+([^)]+)\)', req.read_text()):
            tail += f'  {comp}: [{", ".join(deps.split())}]\n'

    sdk = block_dir / 'sdkconfig.defaults'
    if sdk.is_file():
        tail += '\nsdkconfig:\n'
        for k, v in _sdk_parse(sdk).items():
            tail += f'  {k}: "{v}"\n' if isinstance(v, str) else f'  {k}: {v}\n'

    idf = block_dir / 'idf_component.yml'
    if idf.is_file():
        deps = yaml.safe_load(idf.read_text())['dependencies']
        # yaml-dump, not f-string quoting: a dependency value may be a nested
        # object (conditional `matches:` clauses), not just a version string.
        tail += '\n' + yaml.safe_dump({'idf_components': deps}, default_flow_style=False, sort_keys=False)

    return header + body + tail


def assemble_block(block_dir: pathlib.Path, out_dir: pathlib.Path) -> None:
    """Copy a block dir, expanding the manifest + folding slots/ back in."""
    # Strip the BLOCK-ROOT sidecars only — they are folded into block.yml
    # below. ignore_patterns() is depth-blind, which silently deleted a
    # component-level idf_component.yml (components/app_agents/ carries a real
    # IDF manifest with a git dependency; stripping it broke the build with
    # "Failed to resolve component 'agent'").
    _root = str(block_dir)
    def _ignore(src, names):
        if src != _root:
            return set()
        return {n for n in names if n in ('slots', 'requires.cmake', 'sdkconfig.defaults', 'idf_component.yml')}
    shutil.copytree(block_dir, out_dir, ignore=_ignore)
    manifest = expand_manifest(block_dir)
    (out_dir / 'block.yml').write_text(manifest)
    slots_dir = block_dir / 'slots'
    if not slots_dir.is_dir():
        return

    if not manifest.endswith('\n'):
        manifest += '\n'
    lines = [manifest.rstrip('\n'), '', 'slots:']
    for slot_file in sorted(slots_dir.iterdir()):
        if slot_file.suffix not in ('.c', '.h') or slot_file.name.startswith('.'):
            continue
        name = slot_file.stem
        body = slot_file.read_text()
        # `|` block scalar: value ends with exactly one \n (clip). Preserve
        # other endings with |- (none) / |+ (extra blank lines kept).
        if body.endswith('\n\n'):
            indicator = '|+'
        elif body.endswith('\n'):
            indicator = '|'
        else:
            indicator = '|-'
        lines.append(f'  {name}: {indicator}')
        for ln in body.split('\n')[: -1 if body.endswith('\n') else None]:
            lines.append(f'    {ln}' if ln.strip() else '')
    (out_dir / 'block.yml').write_text('\n'.join(lines) + '\n')


def expand_product(text: str) -> str:
    """Replace `- include: <name>` items with the baseline's instances."""
    def splice(m: re.Match) -> str:
        name = m.group(1).strip()
        blines = (ROOT / 'baselines' / f'{name}.yml').read_text().split('\n')
        start = blines.index('instances:')
        body = [l for l in blines[start + 1:] if l.strip()]
        return '\n'.join(body)
    return re.sub(r'^  - include: (.+)$', splice, text, flags=re.M)


def assemble_products(out: pathlib.Path) -> list[pathlib.Path]:
    src = ROOT / 'product_configurations'
    if out.exists():
        shutil.rmtree(out)
    products = []
    for manifest in sorted(src.rglob('product.yml')):
        rel = manifest.parent.relative_to(src)
        shutil.copytree(manifest.parent, out / rel)
        (out / rel / 'product.yml').write_text(expand_product(manifest.read_text()))
        products.append(out / rel / 'product.yml')
    return products


def assemble_tree(out: pathlib.Path) -> list[pathlib.Path]:
    src = ROOT / 'code_blocks'
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    blocks = []
    for manifest in sorted(src.rglob('block.yml')):
        rel = manifest.parent.relative_to(src)
        assemble_block(manifest.parent, out / rel)
        blocks.append(out / rel / 'block.yml')
    # non-block top-level files (INVENTORY etc. handled by sync separately)
    return blocks


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', type=pathlib.Path)
    ap.add_argument('--products-out', type=pathlib.Path)
    ap.add_argument('--check', action='store_true')
    args = ap.parse_args()

    out = args.out or pathlib.Path(tempfile.mkdtemp()) / 'code_blocks'
    blocks = assemble_tree(out)
    if args.products_out or args.check:
        products = assemble_products(args.products_out or pathlib.Path(tempfile.mkdtemp()) / 'product_configurations')
        for pf in products:
            d = yaml.safe_load(pf.read_text())
            assert 'instances' in d and not any('include' in i for i in d['instances']), pf
        print(f'assembled {len(products)} products (all includes expanded)')

    bad = []
    for b in blocks:
        try:
            d = yaml.safe_load(b.read_text())
            assert isinstance(d, dict) and 'id' in d and 'kind' in d
        except Exception as e:  # noqa: BLE001
            bad.append(f'{b}: {e}')
    if bad:
        print('ASSEMBLY ERRORS:\n  ' + '\n  '.join(bad), file=sys.stderr)
        return 1
    print(f'assembled {len(blocks)} blocks → {out} (all parse clean)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
